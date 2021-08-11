module;
#include <utility>
export module Media;
export import AudioInfo;
export import AudioData;

export struct Media
{
	Media(const char* file, AudioInfo info = AudioInfo::Default()) : name(file), data(std::move(AudioData::FromFile(file))), info(info)
	{}

	const char* name;
	AudioData data;
	AudioInfo info;
};