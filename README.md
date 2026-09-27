# 10MB-video-compressor for Discord V1.2
A little project I made in around 12 days (version 1.1) that automatically compresses mp4 files given to it using hardware-accelerated AV1 encoding on supported vendors into the 20MB file limit for Discord users without Nitro.
It runs in the background without opening any terminal windows.

To use it you need to either drag and drop an MP4 file onto the executable in the Windows File Explorer or open the MP4 with the compiled executable.

OS: Windows 11
Dependencies: Vulkan, FFmpeg

Error code explanation: 

Code 0: No error.
Code 1: Generic error.
Code 2: No file argument was provided, check if you opened the MP4 with the executable.
Code 3: The GPU does not support AV1. (not used in version 1.2)
Code 4: FFmpeg is not in the system path or is not installed.
Code 5: Unknown GPU vendor. (not used in version 1.2)
Code 6: The length of the Video came back as 0 or negative.
Code 7: Failed to run the Vulkan hardware enumeration.
Code 8: The Bitrate was calculated to be over 50 Mb/s.
