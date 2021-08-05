module;
#include <fstream>
#include <filesystem>
export module AudioData;

export struct AudioData
{
	const char* ptr;
	const bool ptrDelete;
	const unsigned int length;

	private:
	void Free() const noexcept
	{
		if (ptrDelete) delete[] ptr;
	}

	public:
	static AudioData FromFile(const char* file)
	{
		if (!std::filesystem::exists(file))
			throw "File does not exist";

		std::ifstream fs(file, std::ifstream::binary);
		const unsigned int fsLength = fs.seekg(0, fs.end).tellg();
		fs.seekg(0, fs.beg);

		char* data = new char[fsLength];
		fs.read(data, fsLength);
		fs.close();
		return { data, true, fsLength };
	}

	friend class AudioPlayer;
};