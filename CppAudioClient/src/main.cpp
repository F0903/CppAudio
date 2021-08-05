import AudioPlayer;

int main()
{
	auto audio = AudioPlayer();
	auto media = Media("./resources/song.pcm");

	audio.Play(media);
}