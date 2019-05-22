#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>

#include <string>

using namespace eosio;
using std::string;

CONTRACT lumetokenctr : public eosio::contract {

  using contract::contract;

  public:

    ACTION create   ( name issuer, asset maximum_supply );
    ACTION burn     ( name from, asset quantity, string memo );
    ACTION issue    ( name to, asset quantity, string memo );
    ACTION transfer ( name from, name to, asset quantity, string memo );

    ACTION stakevote( uint64_t poll_id, uint64_t option, uint64_t amount, string voter );


    inline asset get_supply  ( symbol_code sym )const;
    inline asset get_balance ( name owner, symbol_code sym )const;

  private:
    const eosio::symbol LUMESYMBOL = symbol(symbol_code("LUME"), 3);
    const int64_t LUME_PRECISION_MULTIPLIER = 1000;
    const name POLLS_CONTRACT = name("lumeospollss");

    TABLE account 
    {
        asset balance;

        uint64_t primary_key()const { return balance.symbol.code().raw(); }
    };
    typedef eosio::multi_index<name("accounts"), account> accounts;

    TABLE currency_stats 
    {
        asset          supply;
        asset          max_supply;
        name           issuer;

        uint64_t primary_key()const { return supply.symbol.code().raw(); }
    };
    typedef eosio::multi_index<name("stat"), currency_stats> stats;

    void sub_balance( name owner, asset value );
    void add_balance( name owner, asset value, name ram_payer );

};

asset lumetokenctr::get_supply( symbol_code sym )const
{
    stats statstable( _self, sym.raw() );
    const auto& st = statstable.get( sym.raw() );
    return st.supply;
}

asset lumetokenctr::get_balance( name owner, symbol_code sym )const
{
    accounts accountstable( _self, owner.value );
    const auto& ac = accountstable.get( sym.raw() );
    return ac.balance;
}
