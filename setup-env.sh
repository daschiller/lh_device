# we use the Debian ARM toolchain
#export GNUARMEMB_TOOLCHAIN_PATH=/usr
# export GNUARMEMB_TOOLCHAIN_PATH=~/bin/gcc-arm-none-eabi-9-2019-q4-major
# export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export ZEPHYR_SDK_INSTALL_DIR=~/bin/zephyr-sdk-0.15.1
export ZEPHYR_NRF_MODULE_DIR=~/bin/ncs/nrf

source venv/bin/activate
source ~/bin/ncs/zephyr/zephyr-env.sh
