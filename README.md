# SpotIno

A DIY project to make a functional Spotify controller using an ESP32 with a TFT LCD Module ST7796 3.5' (easy to find on markets like Aliexpress).

>[!warning]
>Compatibility is only guaranteed with this kind of display

## Requirements
- Libraries
    - [TJpg_Decoder](https://github.com/Bodmer/TJpg_Decoder)
    - [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI)
    - [SpotifyArduino](https://github.com/thegeek-sys/spotify-api-arduino)

## Usage
First of all you need to get your Refresh Token by following [this giude](https://github.com/witnessmenow/spotify-api-arduino/tree/main?tab=readme-ov-file#setup-instructions)

Once you have all the keys compile `secrets_sample.h` (needs to be renamed `secrets.h`) in this way:
- `CLIENT_ID` -> put the client id of the app from the guide above
- `CLIENT_SECRET` -> same as before
- `SPOTIFY_REFRESH_TOKEN` -> you have to paste the refres token you generated from `getRefreshToken` example from the guide above
- `SPOTIFY_MARKET` -> market of your Spotify (eg. EN, IT, ...)
- `SSID` -> SSID of your wireless network
- `PASSWORK` -> password of your wireless network

After you have finished compiling your Spotify informations you have to put the right pins for your display according to your configuration inside [`User_Setup.h`](https://github.com/Bodmer/TFT_eSPI/blob/master/User_Setup.h) (my [configuration](https://github.com/thegeek-sys/SpotIno/blob/main/User_Setup.h))

Once you are done you are ready to compile `spot.ino` and upload it!
