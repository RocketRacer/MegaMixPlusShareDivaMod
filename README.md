# MegaMix+ ShareDiva Mod
Mod for Hatsune Miku Project DIVA: MegaMix+ that shares your scores when you complete a song with a Discord bot of the same name. More information about the bot and general usage of this mod can be found in the [ShareDiva documentation](https://try.sharediva.xyz/). There is little point in having this mod enabled without ShareDiva being present in your server.

## Overview

The main meat of the mod is in `Mod.cpp` - upon start up of the game, the mod will attempt to connect to Discord client via a separately setup Discord app in Discord Developer console, and upon successful connection (which is normally when you have a Discord client running alongside the game and have internet connection) it will make a call to Discord API to get currently logged in user's ID. **Note:** the mod only grabs UID of the user and nothing else as the mod does not need anything else from Discord side to successfully identify who is currently playing the game. 

When a user reaches the end-of-song screen, either via successfully completing it or failing it, the mod will first attempt to read the current user's Steam ID via a Steamworks library. It's read via an API once and stored in memory for the rest of the session. **Note:** only Steam ID is collected, as similarly to Discord UID, nothing else is required for the mod to do its thing.

Final part is collecting all information about the song, bundling it into a JSON and sending a POST HTTPS request to ShareDiva Discord Bot's secure end-point. This operation is performed in a separate thread to not hang the game at the score results screen.

## Tools and libraries used
- Microsoft Visual Studio 2022 Community Edition
- [Discord SDK C++ libraries](https://discord.com/developers/docs/game-sdk/sdk-starter-guide)
- [Steamworks API libraries](https://partner.steamgames.com/doc/sdk/api)
- [libcurl](https://curl.se/libcurl/c/)
- [nlohmann's JSON library](https://github.com/nlohmann/json)

## Notes
- There are two addresses for `DivaCurrentPVDifficultyAddress` and one is commented out. The reason being is that when Stewie2.0's SongLimitPatch is enabled, the memory shifts slightly and one address, the difficulty of the song, gets moved to an entirely different place. This results in the switch case failing to POST any scores, as it will always read that memory address as 0, which gets read as Easy diffculty that never gets sent to the bot. The current workaround is to compile a second executable and put it in a release, and ask the users to manually point the mod to the other exectuable, otherwise ShareDiva won't work. Any suggestions on a cleaner solution are more than welcome.
- Three files are included in `Dependencies\RuntimeDependencies` directory. All three of these files **must** be in the ShareDiva mod folder alongside the ShareDiva.dll, otherwise the mod will not work:
	- `curl-ca-bundle.crt` - certificate file to allow libcurl to send an HTTPS request to ShareDiva Discord Bot
	- `discord_game_sdk.dll` - required to capture Discord user's UID
	- `libcurl-x64.dll` - required to send HTTPS request to ShareDiva Discord Bot
- The .png image files included in the repository are previews that are posted in Diva Mod Archive, GameBanana and Discord App Directory (which is still a work in progress as of this writing and is unreleased)

## Special thanks
- Big thank you to Still for the [DivaScoreCap mod](https://github.com/Still34/azura-diva/tree/master/DivaScoreCap) that was used as a starting point for retrieving all the right data to make ShareDiva do its thing
- Every single person that helped test and provided feedback on ShareDiva during its initial development phases (you know who you are :) )
- Project DIVA Modding 2nd Discord server for helping me get through the absolute cluster truck of a maze of developing custom code and finding the right data in right places within the game
