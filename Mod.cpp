#include "pch.h"
#include "Helpers.h"
#include <stdio.h>
#include <cstdint>
#include <SigScan.h>
#include <detours.h>
#include <toml.hpp>
#include <thread>
#include <string>
#include <curl/curl.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <isteamuser.h>
#include <steam_api.h>
#include <inttypes.h>
#include <windows.h>
#include <csignal>
#include "Diva.h"

// MegaMix+ addresses

const int64 ShareDivaBotDiscordId = 1004894321067184169;
const uint64_t DivaScoreBaseAddress = 0x00000001412EF568;
const uint64_t DivaScoreCompletionRateAddress = 0x00000001412EF634;
const uint64_t DivaScoreWorstCounterAddress = 0x00000001416E2D40; // For whatever reason the "worst" counter is stored separately from the rest of the hit counters
const uint64_t DivaScoreGradeAddress = 0x00000001416E2D00;
const uint64_t DivaCurrentPVTitleAddress = 0x00000001412EF228;
const uint64_t DivaCurrentPVIdAddress = 0x00000001412C2340;

// Non-SongLimitPatch 1.02
//const uint64_t DivaCurrentPVDifficultyAddress = 0x00000001412B634C;

// SongLimitPatch 1.02 ONLY
const uint64_t DivaCurrentPVDifficultyAddress = 0x00000001423157AC;

const std::string ApiEndpoint = "<REDACTED>";
const std::string ConfigFileName = "config.toml";
const std::string ConfigDiscordUidName = "discordUid";

uint64 steamId64 = 0;
uint64 discordUid = 0;

int DiscordSdkTimeoutCounterLimit = 10;

bool consoleEnabled = false;

void* DivaScoreTrigger = sigScan(
    "\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x48\x89\x7C\x24\x00\x55\x41\x54\x41\x55\x41\x56\x41\x57\x48\x8B\xEC\x48\x83\xEC\x60\x48\x8B\x05\x00\x00\x00\x00\x48\x33\xC4\x48\x89\x45\xF8\x48\x8B\xF9\x80\xB9\x00\x00\x00\x00\x00\x0F\x85\x00\x00\x00\x00",
    "xxxx?xxxx?xxxx?xxxxxxxxxxxxxxxxxxx????xxxxxxxxxxxx?????xx????"
);

void curlOperation(std::string resultsString)
{
    CURL* curl;
    CURLcode res;

    if (GetConsoleWindow()) {
        if (GetConsoleOutputCP() != CP_UTF8) {
            SetConsoleOutputCP(CP_UTF8);
        }
        consoleEnabled = freopen("CONOUT$", "w", stdout) != NULL;
    }

    if (consoleEnabled)
        printf(resultsString.c_str());

    curl_global_init(CURL_GLOBAL_ALL);

    curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");

        const char* endpoint = ApiEndpoint.c_str();
        curl_easy_setopt(curl, CURLOPT_URL, endpoint);

        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
        curl_easy_setopt(curl, CURLOPT_CAINFO, "mods/ShareDiva/curl-ca-bundle.crt");

        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, resultsString.length());

        // Add the results body to the request
        const char* data = resultsString.c_str();
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);

        // Send request, read the result, print any errors or confirm successful send
        res = curl_easy_perform(curl);

        if (consoleEnabled)
        {
            if (res != CURLE_OK)
            {
                printf("[ShareDiva] ERROR: Failed to send PV results to ShareDiva Bot: %s\n", curl_easy_strerror(res));
            }
            else
            {
                printf("[ShareDiva] Successfully sent PV results to ShareDiva Bot.\n");
            }
        }

        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
}

HOOK(int, __fastcall, _PrintResult, DivaScoreTrigger, int a1) {

    DIVA_SCORE DivaScore = *(DIVA_SCORE*)DivaScoreBaseAddress;
    int DivaScoreWorst = *(int*)DivaScoreWorstCounterAddress;
    DIVA_STAT DivaStat = *(DIVA_STAT*)DivaScoreCompletionRateAddress;
    std::string& DivaTitle = *(std::string*)DivaCurrentPVTitleAddress;
    DIVA_PV_ID DivaPVId = *(DIVA_PV_ID*)DivaCurrentPVIdAddress;
    DIVA_DIFFICULTY DivaDif = *(_DIVA_DIFFICULTY*)DivaCurrentPVDifficultyAddress;
    DIVA_GRADE DivaGrade = *(_DIVA_GRADE*)DivaScoreGradeAddress;

    // Can't be performed in init() as Steam API hasn't initialised within the game by then
    if (steamId64 == 0)
    {
        CSteamID steamId = SteamUser()->GetSteamID();
        steamId64 = steamId.ConvertToUint64();
    }

    // Client-side processing of whether or not to send the results to ShareDiva bot
    bool postScore = false;

    switch (DivaDif)
    {
        case Normal:
            if (DivaStat.CompletionRate >= 50.0F)
                postScore = true;
            break;
        case Hard:
            if (DivaStat.CompletionRate >= 55.0F)
                postScore = true;
            break;
        case Extreme:
        case ExExtreme:
            if (DivaStat.CompletionRate >= 65.0F)
                postScore = true;
            break;
        case Easy:
        default:
            break;
    }

    if (!postScore)
        return original_PrintResult(a1);

    // Create JSON with all results that will be sent to the bot
    nlohmann::json results = {
        {"shareDivaUser", {
            {"steamId", steamId64},
            {"discordUid", discordUid},
        }},
        {"pvId", DivaPVId.Id},
        {"pvName", DivaTitle},
        {"pvDifficulty", DivaDif},
        {"totalScore", DivaScore.TotalScore},
        {"completionRate", DivaStat.CompletionRate},
        {"scoreGrade", DivaGrade},
        {"combo", DivaScore.Combo},
        {"cool", DivaScore.Cool},
        {"fine", DivaScore.Fine},
        {"safe", DivaScore.Safe},
        {"sad", DivaScore.Sad},
        {"worst", DivaScoreWorst}
    };

    // Dump JSON into a string
    std::string resultsString = results.dump();

    // Detach a thread that will be sending the result so the game doesn't hang
    std::thread curlThread(curlOperation, resultsString);
    curlThread.detach();

    return original_PrintResult(a1);
};

extern "C"
{
    namespace {
        volatile bool done{ false };
    }

    struct DiscordState {
        discord::User currentUser;
        std::unique_ptr<discord::Core> core;
    };

    void handleConfigInvalidUidException()
    {
        if (consoleEnabled)
            printf("[ShareDiva] ERROR: Failed to read Discord UID from config file: invalid UID provided.\n");

        MessageBoxA(NULL, "Hello there!\nThe 'discorduid' you provided is not a valid number - it must be a numerical number, such as 123456789123456789. Please check the configuration file and try again.\nThe game will now close.", "ShareDiva configuration error", MB_OK);
        exit(EXIT_SUCCESS);
    }

    void handleConfigMissingUid()
    {
        if (consoleEnabled)
            printf("[ShareDiva] ERROR: Failed to read Discord UID from config file: Discord UID was not provided.\n");

        MessageBoxA(NULL, "Hello there!\n\nIt appears Discord is unable to co-operate with me and I'm unable to retrieve your Discord UID through it. :( I apologise for this inconvenience, but this out your or my control, this is a fault on Discord's end.\n\nPlease update the 'discorduid' value in ShareDiva's 'config.toml' configuration file with your Discord UID.\nShareDiva configuration file can be found in 'mods/ShareDiva/config.toml'.\n\nInstructions on how to retrieve your Discord UID are provided in the README file in mod's folder.\n\nThe game will now close.", "ShareDiva configuration error", MB_OK);
        exit(EXIT_SUCCESS);
    }

    void __declspec(dllexport) Init()
    {
        if (GetConsoleWindow()) {
            if (GetConsoleOutputCP() != CP_UTF8) {
                SetConsoleOutputCP(CP_UTF8);
            }
            consoleEnabled = freopen("CONOUT$", "w", stdout) != NULL;
        }
        
        INSTALL_HOOK(_PrintResult);

        if (consoleEnabled)
            printf("[ShareDiva] Reading the Discord UID from mod config file.\n");

        const toml::value config = toml::parse(ConfigFileName);
        const toml::value discordUid_ = toml::get<toml::table>(config).at(ConfigDiscordUidName);

        try {
            discordUid = toml::get<uint64>(discordUid_);
            if (consoleEnabled)
                printf("[ShareDiva] Using Discord UID retrieved from config file: %" PRIu64 ". If this is incorrect, please update it prior to submitting any song results.\n", discordUid);
        }
        catch (toml::type_error) {
            handleConfigInvalidUidException();
        }
        catch (std::out_of_range) {
            handleConfigInvalidUidException();
        }

        // 0 is default value in the config, meaning that the user hasn't updated their config file
        if (discordUid == 0)
        {
            handleConfigMissingUid();
        }
    }    
}