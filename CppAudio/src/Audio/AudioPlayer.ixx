module;
#include <Windows.h>
#include <Audioclient.h>
#include <fstream>
#include <filesystem>
#include <thread>
export module AudioPlayer;
export import Media;
import AudioWriter;

export class AudioPlayer
{
	public:
	void Play(const Media& media) const
	{
		const auto& data = media.data;
		const auto& info = media.info;

		auto writer = AudioWriter(info);
		writer.Write(data);

		data.Free();
	}
};