#include "stdafx.h"
#include "nemesis.h"

#include <map>
#include <mutex>

namespace
{

std::mutex s_mutex;
std::map<std::string, int> s_deathsBy; // how often each bot has killed the local player
std::string s_nemesis;

} // namespace

void Nemesis::Reset()
{
    std::lock_guard lock{ s_mutex };
    s_deathsBy.clear();
    s_nemesis.clear();
}

Nemesis::Result Nemesis::OnKill(const std::string &attacker, bool attackerBot, const std::string &victim,
    bool victimBot, bool victimLocal, bool attackerLocal, std::string &name)
{
    std::lock_guard lock{ s_mutex };

    if (victimLocal && attackerBot && !attacker.empty())
    {
        const int deaths = ++s_deathsBy[attacker];
        if (deaths < DeathsToBeNamed || attacker == s_nemesis)
        {
            return Result::None;
        }

        // only a bot that has killed you more than the current nemesis takes the title
        if (!s_nemesis.empty() && deaths <= s_deathsBy[s_nemesis])
        {
            return Result::None;
        }

        s_nemesis = attacker;
        name = attacker;
        return Result::Named;
    }

    if (attackerLocal && victimBot && !s_nemesis.empty() && victim == s_nemesis)
    {
        name = s_nemesis;
        s_deathsBy[s_nemesis] = 0;
        s_nemesis.clear();
        return Result::Slain;
    }

    return Result::None;
}

std::string Nemesis::Current()
{
    std::lock_guard lock{ s_mutex };
    return s_nemesis;
}
