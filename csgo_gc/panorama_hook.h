#pragma once

#include <atomic>
#include <mutex>
#include <string>

class ItemSchema;

// Small patches to the game's Panorama UI, done in memory while the game runs.
//
// The UI scripts live in the signed csgo/panorama/code.pbin, so they can't be
// edited on disk. Instead this hooks the UI engine's RunScript (every script
// in a layout file goes through it) and:
//   - adds a "Sell for N stars" entry to the inventory's right-click menu
//   - adds the SkinStop button to the main menu: one big searchable screen
//     with every container the store sells, bought together with its key
//
// Windows only. Everything is skipped (with a console message) if panorama.dll
// doesn't look like the final CS:GO build.
namespace PanoramaHook
{
    // installs once panorama.dll is loaded, safe to call every frame
    void TryInstall();

    // the containers to list, as built by BuildCaseStoreData. sent to the UI
    // as soon as both the data and the main menu exist
    void SetCaseStoreData(std::string data);
    bool HasCaseStoreData();

    // sends pending data to the UI. call from the game's main thread
    void Tick();

    // --- the parts below are plain text processing, testable anywhere ---

    // the sell prices the right-click "Sell" entry shows, by rarity 1..6 (Consumer to Covert) and for knives and gloves
    struct SellPrices
    {
        uint32_t byRarity[7]{}; // index 0 unused
        uint32_t knifeOrGlove{};
        uint32_t stickerPercent{ 50 }; // stickers, patches and graffiti pay this percent of the rarity price
    };

    // JS object {defIndex: stars} of the items sold for a flat price (cases, agents, music kits), from
    // StarShop::BuildSellTable. the Sell entry looks prices up in it
    void SetSellData(std::string table);

    // adds a "Sell for N stars" entry to item_context_entries.js, before the delete entry. selling is what the
    // game's own delete does once the star shop's selling is on: the GC pays for the item it removes.
    // returns false when the delete entry isn't found
    bool AddSellEntry(std::string &source, const SellPrices &prices);

    // makes the inventory page's delete question (inventory.js) a "sell for N stars?" question for items that have a
    // sell price. its OK runs in the page, which stays alive (the right-click menu closes, and takes any popup
    // callback made in it down too). returns false when the page's delete question isn't found
    bool PatchInventoryDelete(std::string &source, const SellPrices &prices);

    // makes the contents list of a case (popup_capability_decodable.js) load each skin's textures before it shows
    // the skin's picture. the game only does that for the one you click, so a skin and weapon pair it has no
    // ready-made picture of (every one in a custom case) stays a blank tile. returns the number of changes
    int PatchCasePopup(std::string &source);

    // finish title -> "#rrggbb", as a JS object. the case popup and the case roll draw a tinted plain weapon for a
    // custom finish's tile (the game has no picture of those). set once from the schema
    void SetKitTints(std::string tintsJsObject);
    std::string BuildKitTints(const ItemSchema &itemSchema);

    // your ranks and level, for the scoreboard (Tab). the scoreboard reads each player's rank from the game server,
    // which has nothing for a listen server, so mock_adapter.js is patched to answer for you and for the bots. bots
    // get a rank and level near yours. only in competitive and wingman matches. returns the number of changes
    struct ScoreboardRanks
    {
        int competitive{}; // your rank, 0 while you are unranked (not enough wins yet)
        int wingman{};
        int level{};
        int botCompetitive{ -1 }; // what the bots' ranks are based on; -1 = your rank
        int botWingman{ -1 };
    };
    int PatchScoreboardRanks(std::string &source, const ScoreboardRanks &ranks);

    // the ranks the scoreboard patch uses, kept up to date by the GC when they change (header only, so the GC
    // doesn't need the UI code to build)
    inline std::atomic<int> g_rankCompetitive{ 0 };
    inline std::atomic<int> g_rankWingman{ 0 };
    inline std::atomic<int> g_playerLevel{ 0 };
    inline std::atomic<int> g_botRankCompetitive{ -1 };
    inline std::atomic<int> g_botRankWingman{ -1 };

    inline void SetScoreboardRanks(const ScoreboardRanks &ranks)
    {
        g_rankCompetitive = ranks.competitive;
        g_rankWingman = ranks.wingman;
        g_playerLevel = ranks.level;
        g_botRankCompetitive = ranks.botCompetitive;
        g_botRankWingman = ranks.botWingman;
    }

    // the end-of-match screens (drops, XP bar, skill group) read their data from the game server, which has nothing for
    // a listen server. the scripts are patched to use this instead: the GC fills it in when the match ends and the
    // next Tick() hands it to the end-of-match page. returns the number of changes
    int PatchMatchEndAdapter(std::string &source);

    // runs after the match's data is set: a banner on the end-of-match page when your rank changed ("RANK UP  Silver I
    // -> Silver II") with a sound. does nothing when there is no rank_change in the data
    const char *RankBannerScript();

    // the final client hides the skill groups on the profile card (_SetAllSkillGroups hides the container and returns
    // before doing anything). this lets it run, so the card shows your competitive, wingman and danger zone ranks.
    // returns 1 when changed
    int PatchPlayerCardRanks(std::string &source);

    // bots show a made-up ping on the scoreboard (steady with a little wobble) instead of the "BOT" label: the adapter
    // answers GetPlayerPing for them and the scoreboard stops printing the label. each returns 1 when changed
    int PatchBotPingAdapter(std::string &source);
    int PatchBotPingLabel(std::string &source);

    // bots get a face on the scoreboard: an agent's picture of their team, picked by name (needs scoreboard.js)
    int PatchBotAvatars(std::string &source);
    // the agents by team as a JS object {ct:[def,..],t:[def,..]}
    std::string BuildBotFaces(const ItemSchema &itemSchema);
    void SetBotFaces(std::string facesJsObject);

    // bots get a clan tag on the scoreboard, the same for the bots of a team, new ones every match (the scoreboard
    // shows GetPlayerClanTag in front of the name). returns 1 when changed
    int PatchBotClanTags(std::string &source);

    // the inventory ignores "inventory updated" while a capability popup (opening a case, stickers, name tags) is
    // counted as open, and that count can get stuck when such a popup is dismissed without telling the page. the
    // list then stays out of date until you leave the inventory and come back. this stops the page ignoring updates
    // for that reason. returns 1 when changed
    int PatchInventoryRefresh(std::string &source);

    // bot difficulty follows your competitive rank (2 easy to 5 expert, as the Play menu numbers them; 0 is No Bots; -1 leaves the Play menu's choice alone). the
    // GC sets g_botDifficulty, Tick() stores it as a game setting and the Play menu script uses it instead of the
    // difficulty dropdown. PatchBotDifficulty makes that change to mainmenu_play.js and returns 1 when changed
    inline std::atomic<int> g_botDifficulty{ -1 };

    // how long the match end screens stay up before the match restarts, in seconds. the listener asks at the start of
    // every round (the game's mode settings put the default back when a match starts) and Tick() sends
    // mp_match_restart_delay to the game console
    inline std::atomic<int> g_restartDelayRequest{ 0 };
    int PatchBotDifficulty(std::string &source);
    int PatchMatchEndGate(std::string &source, const char *readyFlag, const char *kind);

    // {"drops": {...}, "xp": {...}, "skill": {...}} as the screens expect; "{}" for nothing
    inline std::mutex g_matchEndMutex;
    inline std::string g_matchEndJson;
    inline std::atomic<bool> g_matchEndPending{ false };

    inline void SetMatchEndScreens(std::string json)
    {
        std::lock_guard lock{ g_matchEndMutex };
        g_matchEndJson = std::move(json);
        g_matchEndPending = true;
    }

    // lets a context menu entry whose name starts with '!' show that text as it is (the menu would look the
    // name up as a language token). returns the number of changes
    int PatchContextMenuLabels(std::string &source);

    // JS array of [crateDefIndex, keyDefIndex or 0, kind] in store order
    // (weapon cases, souvenir packages, capsules, other; newest first),
    // limited to the kinds enabled in store_containers
    std::string BuildCaseStoreData(const ItemSchema &itemSchema);

    // the Case Store UI script, run in the main menu's context
    const std::string &CaseStoreScript();
}
