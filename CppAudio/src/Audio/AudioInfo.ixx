export module AudioInfo;

export struct AudioInfo
{
	AudioInfo(const unsigned int sampleRate, const unsigned short bitDepth, const unsigned short channels)
		: sampleRate(sampleRate), bitDepth(bitDepth), channels(channels)
	{}

	private:
	mutable unsigned short blockAlign = 0;

	public:
	const unsigned int sampleRate;
	const unsigned short bitDepth;
	const unsigned short channels;

	inline const unsigned short GetBlockAlign() const noexcept
	{
		if (!blockAlign) blockAlign = (channels * bitDepth) / 8;
		return blockAlign;
	}

	static inline AudioInfo Default() noexcept
	{
		return
		{
			48000,
			16,
			2
		};
	}
};