void printCurrentlyPlayingToSerial(CurrentlyPlaying currentlyPlaying) {
	int blocksElapsed = 20*currentlyPlaying.progressMs/currentlyPlaying.durationMs;

	String artists;

	//Serial.println("Artists: ");
	for (int i = 0; i < currentlyPlaying.numArtists; i++) {
		artists += String(currentlyPlaying.artists[i].artistName) + String(", ");
	}
	if (artists.length() > 40) {
		artists = artists.substring(0,37);
		artists += String("...");
	} else {
		artists = artists.substring(0, artists.lastIndexOf(","));
	}

	//if (SPIFFS.exists("/M81.jpg") == true) {
	//	SPIFFS.remove("/M81.jpg");
	//}

	if (song != currentlyPlaying.trackName) {
		song = currentlyPlaying.trackName;
		changedSong = true;
	}

	if (secondsPast*1000 > currentlyPlaying.progressMs) {
		tft.fillRect(tft.width()/2-99, BAR_HEIGHT+1, (int)(pixelPast)-1, 13, altColor);
	}
	pixelPast = (200*((float)currentlyPlaying.progressMs/1000))/((float)currentlyPlaying.durationMs/1000);
	secondsPast = currentlyPlaying.progressMs/1000;
	isPlaying = currentlyPlaying.isPlaying;
	if (!currentlyPlaying.isPlaying) {
		pixelPerSecond = 0.0;
	} else {
		pixelPerSecond = 200/((float)currentlyPlaying.durationMs/1000);
	}

	if (changedSong == true) {
		changedSong = false;
		prevVolume = 101;

		String displaySong = song;
		if (song.indexOf("(") != -1) {
			displaySong = song.substring(0, song.indexOf("(")-1);
		}

		tft.fillScreen(altColor);
		tft.setTextColor(mainColor, altColor);

		uint32_t t = millis();

    String fName = "/"+String(strrchr(currentlyPlaying.albumUri,':')+1)+".jpg";
    Serial.println(fName);

		// Fetch the jpg file from the specified URL, examples only, from imgur
		bool loaded_ok = getFile(currentlyPlaying.albumImages[1].url, fName);

		t = millis() - t;
		imageDelay = t;
		if (loaded_ok) { Serial.print(t); Serial.println(" ms to download"); }

		// List files stored in SPIFFS, should have the file now
		//listSPIFFS();

		t = millis();

		// Now draw the SPIFFS file
		TJpgDec.drawFsJpg(tft.width()/2-80, 20, fName);

		t = millis() - t;
		imageDelay+=t;
		Serial.print(t); Serial.println(" ms to draw to TFT");

		Serial.print("image delay: ");
		Serial.println(imageDelay);
		tft.drawCentreString(displaySong, tft.width()/2, 175, FONT_TYPE);
		tft.drawCentreString(artists, tft.width()/2, 200, FONT_TYPE);
		tft.drawRect(tft.width()/2-100, BAR_HEIGHT, 200, 13, mainColor);
		secondsPast = (currentlyPlaying.durationMs+imageDelay)/1000;
		String seconds = String(secondsPast%60);
		durationSec = currentlyPlaying.durationMs/1000;
		String totTime = String((currentlyPlaying.durationMs+imageDelay)/1000/60) + ":" + (seconds.toInt()<10 ? "0"+seconds : seconds);
		tft.drawString(totTime, tft.width()/2+103, BAR_HEIGHT-6, FONT_TYPE);
		Serial.println("assigned pixel per second: ");
		Serial.print(pixelPerSecond);
		// Time recorded for test purposes


	}
}

bool getPlaylistsCallback(UserPlaylistsResult result, int index, int numPlaylists) {

	Serial.println("--------- Song Details ---------");

	Serial.print("Playlist Index: ");
	Serial.println(index);

	Serial.print("Playlist ID: ");
	Serial.println(result.playlistId);

	Serial.print("Playlist Name: ");
	Serial.println(result.playlistName);

	if (index == 0) {
		tft.setTextColor(altColor);
		tft.fillRect(0, 0, 480, FONT_SIZE, mainColor);
	} else if (index == 1) tft.setTextColor(mainColor);

	maxPlaylists = index;

	tft.drawString(result.playlistName, 3, FONT_SIZE*index, FONT_TYPE);

	Serial.println("-------------------------------");

	return true;
}

bool getResultsCallback(PlaylistResult result, int index, int numResults) {

	Serial.println("--------- Song Details ---------");

	Serial.print("Song Index: ");
	Serial.println(index);

	Serial.print("Song ID: ");
	Serial.println(result.trackUri);

	Serial.print("Song Name: ");
	Serial.println(result.trackName);

	Serial.print("Song Artists: ");
	Serial.println(result.artistName);

	if (index == 0) {
		tft.setTextColor(altColor);
		tft.fillRect(0, 0, 480, FONT_SIZE, mainColor);
	} else if (index == 1) tft.setTextColor(mainColor);



	maxSongs = index;
	tft.drawString(result.trackName, 3, FONT_SIZE*index, FONT_TYPE);
	tft.drawString(result.artistName, tft.width()-tft.textWidth(result.artistName)-3, FONT_SIZE*index, FONT_TYPE);

	Serial.println("-------------------------------");

	return true;
}

void showVolume(int volume) {
	tft.fillRect(tft.width()/2+77, 30, 13, 130, mainColor);
	tft.fillRect(tft.width()/2+89, 10, 40, 170, altColor);
	tft.drawRect(tft.width()/2+77, 30, 13, 130, mainColor);
	tft.fillRect(tft.width()/2+78, 31, 11, (100-volume)*1.3, altColor);
	tft.drawNumber(volume, tft.width()/2+91, ((100-volume)*1.3)+28);
}

void showVolume(PlayerDetails playerDetails) {
	if (prevVolume != playerDetails.device.volumePercent) {
		prevVolume = playerDetails.device.volumePercent;
		tft.fillRect(tft.width()/2+77, 30, 13, 130, mainColor);
		tft.fillRect(tft.width()/2+89, 10, 40, 170, altColor);
		tft.drawRect(tft.width()/2+77, 30, 13, 130, mainColor);
		tft.fillRect(tft.width()/2+78, 31, 11, (100-playerDetails.device.volumePercent)*1.3, altColor);
		tft.drawNumber(playerDetails.device.volumePercent, tft.width()/2+91, ((100-playerDetails.device.volumePercent)*1.3)+28);
	}
}

