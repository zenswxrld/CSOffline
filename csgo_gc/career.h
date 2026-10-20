#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

class CSOEconItem;
class ItemSchema;

// A single-player career log: what you've opened and traded up, which skins of
// each case you've found (collection goals) and the achievements those unlock.
// Goals and achievements pay stars. Kept in csgo_gc/career.txt.
class Career
{
public:
    struct Skin
    {
        uint32_t defIndex{};
        uint32_t paintKit{};

        bool operator<(const Skin &other) const
        {
            return defIndex != other.defIndex ? defIndex < other.defIndex : paintKit < other.paintKit;
        }
    };

    // what a case can give: its regular skins (the collection goal) and how many golds it has
    struct CaseInfo
    {
        std::string name;
        std::vector<Skin> skins;
        uint32_t goldItems{};
        bool custom{};
    };

    struct Pull
    {
        Skin skin;
        uint32_t rarity{};
        bool gold{};
        bool glove{}; // a gold that is gloves rather than a knife
        bool statTrak{};
    };

    // what a match gave: one call each, a headshot on top of its kill
    enum class MatchEvent
    {
        Kill,
        Headshot,
        Mvp,
        Plant,
        Defuse,
        MatchComplete,
    };

    struct Unlock
    {
        std::string title;
        std::string text;
        uint32_t stars{};
        // a collectible the achievement also gives: 0 none, 1 pin, 2 coin, 3 trophy (the harder, the better)
        uint32_t collectible{};
    };

    // what a completed collection pays before the career_rewards percent, and after it
    static constexpr uint32_t GoalReward = 500;
    static uint32_t GoalRewardStars();

    explicit Career(std::string path);

    void Load();
    bool Save() const;

    // each returns what this event newly unlocked (collection goals and achievements)
    std::vector<Unlock> RecordCaseOpened(uint32_t crate, const CaseInfo &info, const Pull &pull);
    std::vector<Unlock> RecordTradeUp(bool goldResult);
    std::vector<Unlock> RecordStarsEarned(uint32_t amount);
    std::vector<Unlock> RecordMatchEvent(MatchEvent event);

    // a won match, counted under a stat such as wins_competitive, wins_wingman, wins_dangerzone or wins_casual
    std::vector<Unlock> RecordMatchWin(const std::string &stat);

    // a lost match (stat is the same "wins_<mode>" name the wins use) and a death of yours, for the stats page
    void RecordMatchLoss(const std::string &stat);
    void RecordDeath();

    // rank points kept as a stat (points_competitive ...); a loss takes points off, never below 0
    void AddRankPoints(const std::string &stat, int64_t delta);

    // the rank (1 = Silver I ... 18 = The Global Elite, or up to maxRank) a number of points is worth, the points
    // a rank starts at, and a rank's name
    static uint32_t RankFromPoints(uint64_t points, uint32_t maxRank);
    static uint64_t PointsForRank(uint32_t rank);
    static const char *RankName(uint32_t rank);

    uint64_t Stat(const std::string &name) const;

    std::string StatsText() const;
    // opened cases (or all of them) with how much of each is collected
    std::string GoalsText(const std::vector<std::pair<uint32_t, CaseInfo>> &cases, bool showAll) const;
    std::string AchievementsText() const;
    // the same data as a JS object, for the SkinStop tabs
    std::string UiData(const std::vector<std::pair<uint32_t, CaseInfo>> &cases) const;

    // the UI data handed from the GC's thread to the main thread that talks to Panorama
    static void Publish(std::string uiData);
    // true (and the data) once per Publish, or again after RepublishLast
    static bool TakePublished(std::string &uiData);
    static void RepublishLast();

    // schema side: the case's name, regular skins and gold count
    static bool BuildCaseInfo(const ItemSchema &schema, uint32_t crate, CaseInfo &info);
    // the pull, from the item an opening produced
    static bool BuildPull(const ItemSchema &schema, const CSOEconItem &item, Pull &pull);

private:
    struct CaseProgress
    {
        uint32_t opened{};
        uint32_t golds{};
        bool complete{};
        std::set<Skin> found;
    };

    std::vector<Unlock> CheckAchievements();

    std::string m_path;
    std::map<std::string, uint64_t> m_stats;
    std::map<uint32_t, CaseProgress> m_cases;
    std::map<std::string, uint32_t> m_achievements; // id -> unix time unlocked
    std::set<Skin> m_goldFinishes;                   // every knife and glove finish pulled
    std::set<uint32_t> m_knifeTypes;
    std::set<uint32_t> m_gloveTypes;
    uint32_t m_unsavedStarEvents{};
};
