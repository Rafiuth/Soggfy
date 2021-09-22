# Soggfy - SpotifyOggDumper

Previous Spotify downloader projects ([XSpotify](https://web.archive.org/web/20200303145624/https://github.com/meik97/XSpotify), spotifykeydumper) worked by recovering encryption keys and re-downloading tracks manually from Spotify's CDN. This project takes a slightly different approach - by hooking directly into functions that demux the (already decrypted) OGG stream, a replica of the track can be reconstructed without any loss. Metadata is fetched from Spotify's public API, using the client's access token.

# Notes
- Tracks are dumped in real time (seeking or skipping is inherently unsupported)
- Last supported version: 1.1.68.632