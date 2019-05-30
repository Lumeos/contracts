#include "lumetokenctr.hpp"

void lumetokenctr::create( name issuer, asset maximum_supply )
{
    require_auth( _self );

    auto sym = maximum_supply.symbol;
    eosio_assert( sym.is_valid(),               "invalid symbol name" );
    eosio_assert( maximum_supply.is_valid(),    "invalid supply");
    eosio_assert( maximum_supply.amount > 0,    "max-supply must be positive");

    stats statstable( _self, sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    eosio_assert( existing == statstable.end(), "token with symbol already exists" );

    statstable.emplace( _self, [&]( auto& s ) {
        s.supply.symbol = maximum_supply.symbol;
        s.max_supply    = maximum_supply;
        s.issuer        = issuer;
    });
}

void lumetokenctr::issue( name to, asset quantity, string memo )
{
    auto sym = quantity.symbol;
    eosio_assert( sym.is_valid(),                       "invalid symbol name" );
    eosio_assert( memo.size() <= 256,                   "memo has more than 256 bytes" );

    auto sym_name = sym.code().raw();
    stats statstable( _self, sym_name );
    auto existing = statstable.find( sym_name );
    eosio_assert( existing != statstable.end(),         "token with symbol does not exist, create token before issue" );

    const auto& st = *existing;

    require_auth( st.issuer );
    eosio_assert( quantity.is_valid(),                  "invalid quantity" );
    eosio_assert( quantity.amount > 0,                  "must issue positive quantity" );

    eosio_assert( quantity.symbol == st.supply.symbol,  "symbol precision mismatch" );
    eosio_assert( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply += quantity;
    });

    add_balance( st.issuer, quantity, st.issuer );

    if( to != st.issuer ) {
        SEND_INLINE_ACTION( *this, transfer, { {st.issuer, "active"_n} },
                            { st.issuer, to, quantity, memo }
      );
    }
}

void lumetokenctr::transfer( name from, name to, asset quantity, string memo )
{
    // TODO: don't allow transfer of tokens until April 1st.
    eosio_assert( from != to,        "cannot transfer to self" );

    require_auth( from );

    eosio_assert( is_account( to ),  "to account does not exist");

    auto sym = quantity.symbol.code();
    stats statstable( _self, sym.raw() );
    const auto& st = statstable.get( sym.raw() );

    require_recipient( from );
    require_recipient( to );

    eosio_assert( quantity.is_valid(),                 "invalid quantity" );
    eosio_assert( quantity.amount > 0,                 "must transfer positive quantity" );
    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( memo.size() <= 256,                  "memo has more than 256 bytes" );

    sub_balance( from, quantity );
    add_balance( to,   quantity, from );
}

void lumetokenctr::burn( name from, asset quantity, string memo )
{
     require_auth( from );

     auto sym = quantity.symbol;
     eosio_assert( sym.is_valid(),                      "invalid symbol name" );
     eosio_assert( memo.size() <= 256,                  "memo has more than 256 bytes" );

     auto sym_name = sym.code().raw();
     stats statstable( _self, sym_name );
     auto existing = statstable.find( sym_name );
     eosio_assert( existing != statstable.end(),        "token with symbol does not exist" );

     const auto& st = *existing;

     eosio_assert( quantity.is_valid(),                 "invalid quantity" );
     eosio_assert( quantity.amount > 0,                 "must burn positive quantity" );
     eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );

     statstable.modify( st, same_payer, [&]( auto& s ) {
        s.supply -= quantity;
     });

     sub_balance( from, quantity );
}

void lumetokenctr::stakevote( uint64_t poll_id, uint64_t option, uint64_t amount, name voter ) {
    require_auth(voter);

    eosio_assert(amount > 0, "must transfer a positive amount");

    // Transfer the LUME to the lumeospollss contract for staking
    asset lumeAssetPack = asset(amount * LUME_PRECISION_MULTIPLIER, LUMESYMBOL);
    action(
        permission_level{ voter , name("active") }, 
        _self , name("transfer"),
        std::make_tuple( voter, POLLS_CONTRACT, lumeAssetPack, std::string("stake for vote"))
    ).send();

    // Create the vote in the lumeospollss contract
    action(
        permission_level{ POLLS_CONTRACT, name("active") }, 
        POLLS_CONTRACT, name("vote"),
        std::make_tuple( poll_id, option, amount, voter )
    ).send();
}

void lumetokenctr::sub_balance( name owner, asset value )
{
   accounts from_acnts( _self, owner.value );

   const auto& from = from_acnts.get( value.symbol.code().raw(), "no balance object found" );
   eosio_assert( from.balance.amount >= value.amount, "overdrawn balance" );


   if( from.balance.amount == value.amount ) {
      from_acnts.erase( from );
   } else {
      from_acnts.modify( from, owner, [&]( auto& a ) {
          a.balance -= value;
      });
   }
}

void lumetokenctr::add_balance( name owner, asset value, name ram_payer )
{
   accounts to_acnts( _self, owner.value );
   auto to = to_acnts.find( value.symbol.code().raw() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
      });
   } else {
      to_acnts.modify( to, same_payer, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

EOSIO_DISPATCH( lumetokenctr, (create)(burn)(issue)(transfer)(stakevote) )
