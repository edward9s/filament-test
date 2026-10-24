#!/usr/bin/env python3
"""Fetch the public Vulkan platform headers omitted from Filament's prebuilt SDK."""
import concurrent.futures
import hashlib
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
BASE = 'https://raw.githubusercontent.com/google/filament/v1.78.1/'
# Git blob IDs from the same v1.78.1 source tree as the native SDK.
HEADERS = {
    "utils/api_level.h": "9feb65707600b9566a3502ceb7ac2d4d6061ae9e",
    "bluevk/BlueVK.h": "92e2161b782e7c1f6ff736e90db3513411848d32",
    "vulkan/vk_icd.h": "59204a3419f53b7e4a3610377791d52705a2c65e",
    "vulkan/vk_layer.h": "19d88fce4bac25a66ced1cb79a909dd702d2ab88",
    "vulkan/vk_platform.h": "0ecd4f64719968f57fabbc40b5d884fb6483fd39",
    "vulkan/vulkan.h": "ef94006bb319d65f46ef228af8a8804939352dfc",
    "vulkan/vulkan_android.h": "61ff40ba8cd29891e2778a76b31b0b5911d1036c",
    "vulkan/vulkan_beta.h": "df18b4042b177db542d3161d346f4ce9a21ae267",
    "vulkan/vulkan_core.h": "e6c16498af5373040e5ec3cb8af821dc6a3572be",
    "vulkan/vulkan_directfb.h": "f06f80b70e377358a22d0534910a9ae068461346",
    "vulkan/vulkan_fuchsia.h": "f60907d10b430922bcc1e15ecf60c977008d7889",
    "vulkan/vulkan_ggp.h": "0a8863a14b147a85c78ddcae2f2152b648756bc9",
    "vulkan/vulkan_ios.h": "22ed2c039a6b2308c78eba35b150f620dadebf9b",
    "vulkan/vulkan_macos.h": "a7f5613a05986d0be9ca8791994895f2ca28ae1d",
    "vulkan/vulkan_metal.h": "89a557490647db38f8fc3c0fd7157d9b081e9b30",
    "vulkan/vulkan_screen.h": "7e84d4d9656f1b9d3757e950aa0cda064272a960",
    "vulkan/vulkan_vi.h": "c145f4a800fafe6bfcf8a6bf902bd302775aa907",
    "vulkan/vulkan_wayland.h": "ec706a114b51285da2421a2a00d6321ee213d8f0",
    "vulkan/vulkan_win32.h": "d7a0b2bab4f17a04f3240e38a94c61873d2db5ec",
    "vulkan/vulkan_xcb.h": "cdf6b5269f14b18cfea9d86f486d84a01a51e0d1",
    "vulkan/vulkan_xlib.h": "b3c3e27d7775807cfe93e676905d2d7b04e4c390",
    "vulkan/vulkan_xlib_xrandr.h": "8e99190b4ffe480afea201909daef3e542fa93d9",
    "vk_video/vulkan_video_codec_av1std.h": "8ce283e8a8a4fab73f1ee57484ede50c08c24b9f",
    "vk_video/vulkan_video_codec_av1std_decode.h": "6b8130cd9fcf71c8488dc00ccb6fa0d981ea3adc",
    "vk_video/vulkan_video_codec_h264std.h": "6d27af37b73342388f3281a70c8af34efe1de567",
    "vk_video/vulkan_video_codec_h264std_decode.h": "439cb885e71b7dcbde9b477b2bfb3fd4613bec10",
    "vk_video/vulkan_video_codec_h264std_encode.h": "9e24aa5d991d3b4388b4bd297d66faf46a698f7a",
    "vk_video/vulkan_video_codec_h265std.h": "d0a1bacbea378c7e0acc1ebcf2596da80525bfe9",
    "vk_video/vulkan_video_codec_h265std_decode.h": "0178793e5111ac4eea8f26185bea2312ce645dd3",
    "vk_video/vulkan_video_codec_h265std_encode.h": "ee34491f4e9c0a5fa3740193802ecd437f93473f",
    "vk_video/vulkan_video_codecs_common.h": "5e6ef1db48ee560e2004388f95d25c6a5bc6a19b",
}


def matches(data, blob):
    prefix = b'blob ' + str(len(data)).encode('ascii') + b'\0'
    return hashlib.sha1(prefix + data).hexdigest() == blob


def fetch(entry):
    name, blob = entry
    target = ROOT / '.deps/headers/include' / name
    if target.is_file() and matches(target.read_bytes(), blob):
        return
    target.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=target.parent) as temp:
        download = pathlib.Path(temp) / 'header'
        subprocess.run(['curl', '-fLsS', '--retry', '3', '--connect-timeout', '20',
                        '--max-time', '90', BASE + ('libs/utils/include/' if name.startswith('utils/') else 'libs/bluevk/include/') + name, '-o', str(download)], check=True)
        if not matches(download.read_bytes(), blob):
            raise RuntimeError('Filament Vulkan header checksum mismatch: ' + name)
        download.replace(target)


if __name__ == '__main__':
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
        list(pool.map(fetch, HEADERS.items()))
    print('Verified pinned Filament Vulkan headers')
