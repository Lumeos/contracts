#include "polls.hpp"

void polls::rmrfpolls() {
    eosio_assert(has_auth(_self), "missing required authority of a lumeos admin account");

    std::vector<uint64_t> keysForDeletion;

    eosio::print("rmrfpolls");
    int i = 0;
    for(auto& item : _polls) {
        if (i++ < 50) {
            keysForDeletion.push_back(item.key);   
        } else {
            break;
        }
    }
    
    // now delete each item for that poll
    for (uint64_t key : keysForDeletion) {
        auto itr = _polls.find(key);
        if (itr != _polls.end()) {
            _polls.erase(itr);
        }
    }
}

void polls::rmrfvotes() {
    eosio_assert(has_auth(_self), "missing required authority of a lumeos admin account");
 
    std::vector<uint64_t> keysForDeletion;

    int i = 0;
    for(auto& item : _votes2) {
        if (i++ < 50) {
            keysForDeletion.push_back(item.key);   
        } else {
            break;
        }
    }
    
    // now delete each item for that poll
    for (uint64_t key : keysForDeletion) {
        auto itr = _votes2.find(key);
        if (itr != _votes2.end()) {
            _votes2.erase(itr);
        }
    }
}

void polls::rmrfcomments() {
    eosio_assert(has_auth(_self), "missing required authority of a lumeos admin account");

    std::vector<uint64_t> keysForDeletion;

    int i = 0;
    for(auto& item : _comments) {
        if (i++ < 50) {
            keysForDeletion.push_back(item.key);   
        } else {
            break;
        }
    }
    
    // now delete each item for that poll
    for (uint64_t key : keysForDeletion) {
        auto itr = _comments.find(key);
        if (itr != _comments.end()) {
            _comments.erase(itr);
        }
    }
}

void polls::addpoll(eosio::name s, uint64_t pollId, uint64_t communityId) {
    eosio_assert(has_auth(s), "missing required authority of a lumeos user account");

    auto itr = _polls.find(pollId);
    if (itr != _polls.end()) {
        eosio::print("Poll with id already exists: ", pollId);
        return; // if poll id already exists, stop
    }

    // update the table to include a new poll
    _polls.emplace(s, [&](auto& p) {
        p.key = pollId;
        p.communityId = communityId;
        p.pollStatus = 0;
        p.voteCounts = 0;
        p.commentsCount = 0;
        p.likes = 0;
        p.dislikes = 0;
    });
}

void polls::rmpoll(uint64_t pollId) {
    eosio_assert(has_auth(_self), "missing required authority of a lumeos admin account");
        
    auto foundPoll = _polls.find(pollId);
    if (foundPoll != _polls.end()) {
        _polls.erase(foundPoll);
    }

    std::vector<uint64_t> keysForDeletionFromVotes;
    // find items which are for the named poll
    for(auto& item : _votes2) {
        if (item.pollId == pollId) {
            keysForDeletionFromVotes.push_back(item.key);   
        }
    }
    
    // now delete each item for that poll
    for (uint64_t key : keysForDeletionFromVotes) {
        eosio::print("remove from _votes2 ", key);
        auto itr = _votes2.find(key);
        if (itr != _votes2.end()) {
            _votes2.erase(itr);
        }
    }
}

// Users have to trigger this action through the lumetokenctr::stakevote action
void polls::vote(uint64_t poll_id, uint64_t option, uint64_t amount, name voter) {
    eosio_assert(has_auth(_self), "missing required authority of a lumeos admin account");

    auto foundPoll = _polls.find(poll_id);
    if (foundPoll == _polls.end()) { // did not find poll
        eosio::print("Poll with id does not exist: ", poll_id);
        return;
    } else { // found poll
        if (foundPoll->pollStatus != 1) {
            eosio::print("Can only vote on open polls");
            return;
        }
    }
    // at this point the poll is found and opne

    // has account name already voted?  
    for(auto& vote : _votes2) {
        if (vote.pollId == poll_id && vote.account == voter) {
            return;
        }
    }

    // find the poll and the option and increment the count

    _polls.modify(foundPoll, voter, [&](auto& p) {
        p.voteCounts = p.voteCounts + 1;
    });

    // record that voter has voted
    _votes2.emplace(voter, [&](auto& pv){
        pv.key = _votes2.available_primary_key();
        pv.pollId = poll_id;
        pv.option = option;
        pv.account = voter;
    });
}

void polls::uppolllikes(uint64_t pollId, uint32_t likesCount, uint32_t dislikesCount, name accountName) {
    eosio_assert(has_auth(_self) || has_auth(accountName), "missing required authority of a lumeos user account");

    auto foundPoll = _polls.find(pollId);
    if (foundPoll == _polls.end()) { // did not find poll
        eosio::print("Poll with id does not exist: ", pollId);
        return;
    }
    _polls.modify(foundPoll, accountName, [&](auto& p) {
        p.likes = likesCount;
        p.dislikes = dislikesCount;
    });
}

void polls::addcomment(uint64_t pollId, uint64_t commentId, name accountName) {
    eosio_assert(has_auth(_self) || has_auth(accountName), "missing required authority of a lumeos user account");

    auto foundPoll = _polls.find(pollId);
    if (foundPoll == _polls.end()) { // did not find poll
        eosio::print("Poll with id does not exist: ", pollId);
        return;
    }

    auto foundComment = _comments.find(commentId);
    if (foundComment != _comments.end()) {
        eosio::print("Comment with id already exist: ", commentId);
        return;
    }

    _comments.emplace(accountName, [&](auto& cmnt){
        cmnt.key = commentId;
        cmnt.pollId = pollId;
        cmnt.account = accountName;
        cmnt.likes = 1;
        cmnt.dislikes = 0;
    });

    _polls.modify(foundPoll, accountName, [&](auto& p) {
        p.commentsCount = p.commentsCount + 1;
    });
}

void polls::upcmntlikes(uint64_t commentId, uint32_t likesCount, uint32_t dislikesCount, name accountName) {
    eosio_assert(has_auth(_self) || has_auth(accountName), "missing required authority of a lumeos user account");

    auto foundComment = _comments.find(commentId);
    if (foundComment == _comments.end()) {
        eosio::print("Comment with id does not exist: ", commentId);
        return;
    }

    _comments.modify(foundComment, accountName, [&](auto& c) {
        c.likes = likesCount;
        c.dislikes = dislikesCount;
    });
}

EOSIO_DISPATCH( polls, (rmrfpolls)(rmrfvotes)(rmrfcomments)(addpoll)(rmpoll)(vote)(uppolllikes)(addcomment)(upcmntlikes))
