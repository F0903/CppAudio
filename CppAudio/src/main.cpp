import AudioPlayer;

int main()
{
	auto audio = AudioPlayer();
	audio.Play("./resources/test_s16le.raw");
}