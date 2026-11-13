.PHONY: all core gui kde check install clean
JOBS ?= 2
all: core
core:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_SYSCONFDIR=/etc -DSLAYER_BUILD_GUI=OFF -DSLAYER_BUILD_KCM=OFF
	cmake --build build -j$(JOBS)
gui:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_SYSCONFDIR=/etc -DSLAYER_BUILD_GUI=ON -DSLAYER_BUILD_KCM=OFF
	cmake --build build -j$(JOBS)
kde:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_SYSCONFDIR=/etc -DSLAYER_BUILD_KCM=ON -DSLAYER_BUILD_GUI=ON
	cmake --build build -j$(JOBS)
check:
	ctest --test-dir build --output-on-failure
install:
	cmake --install build
clean:
	rm -rf build
