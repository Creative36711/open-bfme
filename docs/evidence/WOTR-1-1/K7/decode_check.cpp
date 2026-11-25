#include "GameClient/CursorFile.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		std::fprintf(stderr, "usage: %s <file.cur>\n", argv[0]);
		return 2;
	}
	std::ifstream in(argv[1], std::ios::binary);
	std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	CursorImage image;
	std::string error;
	const bool ok = DecodeCursorFile(bytes, image, &error);
	std::printf("DecodeCursorFile: %s\n", ok ? "ok" : "FAILED");
	if (!ok)
	{
		std::printf("error: %s\n", error.c_str());
		return 1;
	}
	std::printf("width=%d height=%d hotX=%d hotY=%d frames=%zu sequence=%zu jiffies=%zu\n",
		image.width, image.height, image.hotX, image.hotY, image.frames.size(), image.sequence.size(), image.jiffies.size());
	const std::vector<std::uint8_t> &frame = image.frames[0];
	for (int y = 0; y < image.height; ++y)
	{
		for (int x = 0; x < image.width; ++x)
		{
			const std::uint8_t *p = frame.data() + ((std::size_t)y * image.width + x) * 4;
			const char mark = p[3] == 0 ? '.' : (p[0] > 127 ? '#' : 'o');
			std::putchar(mark);
		}
		std::putchar('\n');
	}
	return 0;
}
