/* SPDX-License-Identifier: MIT */
#ifndef SLAYER_COLOR_MODEL_H
#define SLAYER_COLOR_MODEL_H
#define SLAYER_COLOR_MODEL_MAX (8 + 32 * 6 + 128 * 4)
struct slayer_color_vec { long long x, y, z; };
static inline struct slayer_color_vec slayer_color_delta(const unsigned char *a, const unsigned char *b)
{
    struct slayer_color_vec v = {(int)a[0]-(int)b[0], (int)a[1]-(int)b[1], (int)a[2]-(int)b[2]};
    return v;
}
static inline long long slayer_color_det(struct slayer_color_vec a, struct slayer_color_vec b, struct slayer_color_vec c)
{
    return a.x*(b.y*c.z-b.z*c.y)-a.y*(b.x*c.z-b.z*c.x)+a.z*(b.x*c.y-b.y*c.x);
}
static inline const unsigned char *slayer_color_vertex(const unsigned char *model, unsigned int index)
{ return model + 8 + index * 6; }
static inline long long slayer_color_volume(const unsigned char *model, const unsigned char *cell)
{
    const unsigned char *a = slayer_color_vertex(model, cell[0]);
    return slayer_color_det(slayer_color_delta(slayer_color_vertex(model,cell[1]),a),
        slayer_color_delta(slayer_color_vertex(model,cell[2]),a),slayer_color_delta(slayer_color_vertex(model,cell[3]),a));
}
static inline int slayer_color_model_valid(const unsigned char *model, unsigned int size)
{
    unsigned int n, t, i, j, k, corners = 0, used = 0;
    long long volume = 0;
    if (size < 8 || size > SLAYER_COLOR_MODEL_MAX || model[0]!='S' || model[1]!='C' ||
        model[2]!='3' || model[3]!='D' || model[4]!=1 || model[7]) return 0;
    n=model[5];t=model[6];
    if (n<8 || n>32 || !t || t>128 || size!=8+n*6+t*4) return 0;
    for(i=0;i<n;++i){
        const unsigned char *v=slayer_color_vertex(model,i);
        if ((!v[0] || v[0]==255) && (!v[1] || v[1]==255) && (!v[2] || v[2]==255))
            corners |= 1u << ((v[0]==255) | ((v[1]==255)<<1) | ((v[2]==255)<<2));
        if(!v[0] && !v[1] && !v[2] && (v[3] || v[4] || v[5])) return 0;
        if(v[0]==255 && v[1]==255 && v[2]==255 && (!v[3] || !v[4] || !v[5]))return 0;
        for(j=0;j<i;++j){const unsigned char *b=slayer_color_vertex(model,j);if(v[0]==b[0]&&v[1]==b[1]&&v[2]==b[2])return 0;}
    }
    if(corners!=255)return 0;
    for(i=0;i<t;++i){
        const unsigned char *cell=model+8+n*6+i*4;
        long long value;
        for(j=0;j<4;++j){if(cell[j]>=n)return 0;used|=1u<<cell[j];for(k=0;k<j;++k)if(cell[j]==cell[k])return 0;}
        value=slayer_color_volume(model,cell);if(!value)return 0;
        volume+=value<0?-value:value;
    }
    return used==(n==32?0xffffffffu:(1u<<n)-1) && volume==6LL*255*255*255;
}
/* Integer barycentric interpolation: convex weights keep every result in
 * 0..255. No floating point, allocation, polling or per-frame userspace work. */
static inline int slayer_color_model_apply(const unsigned char *model, const unsigned char *rgb, unsigned char *out)
{
    unsigned int i, channel, n=model[5], t=model[6];
    for(i=0;i<n;++i){const unsigned char *v=slayer_color_vertex(model,i);if(v[0]==rgb[0]&&v[1]==rgb[1]&&v[2]==rgb[2]){for(channel=0;channel<3;++channel)out[channel]=v[3+channel];return 1;}}
    for(i=0;i<t;++i){
        const unsigned char *cell=model+8+n*6+i*4;
        const unsigned char *a=slayer_color_vertex(model,cell[0]);
        struct slayer_color_vec b=slayer_color_delta(slayer_color_vertex(model,cell[1]),a);
        struct slayer_color_vec c=slayer_color_delta(slayer_color_vertex(model,cell[2]),a);
        struct slayer_color_vec d=slayer_color_delta(slayer_color_vertex(model,cell[3]),a);
        struct slayer_color_vec x=slayer_color_delta(rgb,a);
        long long den=slayer_color_det(b,c,d),w[4];
        unsigned int j;
        w[1]=slayer_color_det(x,c,d);w[2]=slayer_color_det(b,x,d);w[3]=slayer_color_det(b,c,x);w[0]=den-w[1]-w[2]-w[3];
        if(den<0){den=-den;for(j=0;j<4;++j)w[j]=-w[j];}
        if(w[0]<0||w[1]<0||w[2]<0||w[3]<0)continue;
        /* Nonnegative barycentric weights keep this convex sum in 0..255. */
        for(channel=0;channel<3;++channel){long long sum=0;for(j=0;j<4;++j)sum+=w[j]*slayer_color_vertex(model,cell[j])[3+channel];out[channel]=(unsigned char)((sum+den/2)/den);}
        return 1;
    }
    return 0;
}
#endif
