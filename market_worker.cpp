//---------------------------------------------------------------------------
//
// Сливает с сервера в файл архивные данные по торговле за определенный период
//
//---------------------------------------------------------------------------

//#include <QCoreApplication>
#include <QThread>
//#include <QFile>
//#include <QTextStream>

#include <vector>
#include <map>
//#include <utility>
#include <fstream>
#include <iostream>
#include <locale>
#include <string>
//#include <sstream>
//#include <limits>
//#include <memory>
#include <ctime>
//#include <iomanip>
//#include <stdlib.h>
//#include <libgen.h>
//#include <algorithm>

#include <binacpp.h>
#include <binacpp_websocket.h>
#include <json/json.h>

#include <lib/sqlite_orm/sqlite_orm.h>


// Some code to make terminal to have colors
#define KGRN "\033[0;32;32m"
#define KCYN "\033[0;36m"
#define KRED "\033[0;32;31m"
#define KYEL "\033[1;33m"
#define KBLU "\033[0;32;34m"
#define KCYN_L "\033[1;36m"
#define KBRN "\033[0;33m"
#define RESET "\033[0m"

#define ITEMSOF(a)  (sizeof(a)/sizeof((a)[0]))

//---------------------------------------------------------------------------

using namespace std;

//---------------------------------------------------------------------------

// хитрая хрень для замены десятичного разделителя в стд стримах
template <class charT, charT sep>
class punct_facet: public std::numpunct<charT> {
protected:
    charT do_decimal_point() const {
        return sep;
    }
};

template<typename T> T from_string(const std::string &str)
{
    std::stringstream ss(str);
    T t;
    ss >> t;
    return t;
}

template<> std::string from_string(const std::string &str)
{
    return str;
}

//---------------------------------------------------------------------------

// вываливо
void ShowMessageAndExit(char * msg, int exit_code)
{
    cout << msg << endl;
    cout << "Press any key to exit" << endl;
    cin.get();	// DEBUG!!!
    exit(exit_code);
}

void SaveKlinesCache(string filename, map<time_t, map<string, double>> &klines_cache)
{
    cout << "SaveKlinesCache to '" << filename << "'" << endl;
    filebuf fb;
    fb.open(filename.c_str(), ios::out);

    ostream out(&fb);

//    out.precision(8);

    // инициализация локали с заменой десятичного разделителя на ','
    locale loc_comma = locale(cout.getloc(), new punct_facet<char, ','>);
    // применение локали к стриму
    out.imbue(loc_comma);

    // print header
    out << "open time;open cost;high cost;low cost;close cost;volume" << endl;

    map<time_t, map<string,double>>::iterator it_i;
    for (it_i = klines_cache.begin() ; it_i != klines_cache.end() ; it_i++) {
        time_t start_of_candle = (*it_i).first/1000LL;
        map<string, double> candle_obj = (*it_i).second;

        out << start_of_candle << ";";
        out << candle_obj["o"] << ";";
        out << candle_obj["h"] << ";";
        out << candle_obj["l"] << ";";
        out << candle_obj["c"] << ";";
        out << candle_obj["v"] << endl;
    }

    fb.close();

    cout << "SaveKlinesCache: done" << endl;
}

string GetFixedDateTimeString(const string &str)
{
    string copy_str = str;

    string::size_type found = copy_str.find_first_of(".:");
    while (found != std::string::npos) {
        copy_str.erase(found, 1);
        found = copy_str.find_first_of(".:", found);
    }
    copy_str.find_first_of(" ");
    while (found != std::string::npos) {
        copy_str[found] = '_';
        found = copy_str.find_first_of(" ", found + 1);
    }

//    cout << "GetFixedDateTimeString: '" << str << "' -> " << copy_str << endl;

    return copy_str;
}

#ifdef _WIN32
#include <windows.h>

bool UTCToUnixTimeWin32(SYSTEMTIME const * stime, std::time_t * t)
{
    static SYSTEMTIME const t1970 = {
        1970, 1, 0, 1, 0, 0, 0, 0
    };

    FILETIME ftc, fts;
    if(::SystemTimeToFileTime(stime, &ftc)
    && ::SystemTimeToFileTime(&t1970, &fts))
    {
        ULARGE_INTEGER ltc = { { ftc.dwLowDateTime, ftc.dwHighDateTime } };
        ULARGE_INTEGER lts = { { fts.dwLowDateTime, fts.dwHighDateTime } };
        if(ltc.QuadPart >= lts.QuadPart)
        {
            *t = std::time_t( (ltc.QuadPart - lts.QuadPart) / 10000000 );
            return true;
        }
    }
    return false;
}
#endif

time_t GetTimeInUTC(string const &x)
{
    time_t tret = -1;

    int day,  month,  year;
    int hour, minute, second;
    if(sscanf(x.c_str(), "%d.%d.%d %d:%d:%d"
                   , &day, &month, &year
                   , &hour, &minute, &second) == 6)
    {
#ifdef _WIN32
        SYSTEMTIME const stime = {
            year / 100 == 0 ? 2000 + year : year, month, 0, day
          , hour, minute, second, 0
        };
        UTCToUnixTimeWin32(&stime, &tret);
#else
        tm dt = {};
        dt.tm_year = (year / 100 == 0 ? 2000 + year : year) - 1900;
        dt.tm_mon  = month - 1;
        dt.tm_mday = day;
        dt.tm_hour = hour;
        dt.tm_min  = minute;
        dt.tm_sec  = second;

        tret = ::timegm(&dt);
#endif
    }
    return tret;
}

long CalcTimeIncrement(string interval_str)
{
    const long time_1m = 60;
    const long time_1h = time_1m * 60;

    if (interval_str == "3m")
        return time_1m * 3;
    if (interval_str == "5m")
        return time_1m * 5;
    if (interval_str == "15m")
        return time_1m * 15;
    if (interval_str == "30m")
        return time_1m * 30;
    if (interval_str == "1h")
        return time_1h;
    if (interval_str == "2h")
        return time_1h * 2;
    if (interval_str == "4h")
        return time_1h * 4;
    if (interval_str == "6h")
        return time_1h * 6;
    if (interval_str == "12h")
        return time_1h * 12;

    if (interval_str == "1d")
        return time_1h * 24;

    if (interval_str == "1w")
        return time_1h * 24 * 7;

    return 0;
}

#define API_KEY 		"AePhdHDp8oHfqruyxaXuUnl7w7QAcilwkgPCv1AaaXnTpckx97CEXynKm9sqqHiR"
#define SECRET_KEY		"Ds1W1w6VgUdHu5brQhLwzJ50EXwxaWxXfBav4v0VAsL7NRpgQ8jzPXwmLuuvNVb7"

//---------------------------------------------------------------------------

struct CandlData {
    time_t open_time;
    double open;
    double high;
    double low;
    double close;
    double volume;
};


struct TPairData {
    string pair_name;   // TODO: enum
    int num_intervals;
    string *intervals_data;
};

struct TMarketData {
    string market_name;   // TODO: enum
    int num_pairs;
    const struct TPairData *pairs_data;
    unsigned long max_bars_for_request;
    unsigned long requests_freezytime;
    // TODO: get_market_time_func
};


//---------------------------------------------------------------------------

string BNC_BTCUSDT_IntervalsData[] = {
//    "5m",
//    "15m",
    "30m",
//    "1h",
//    "2h",
    "4h",
    "6h",
//    "12h",
//    "1d",
//    "1w",
};

string BNC_BNBUSDT_IntervalsData[] = {
//    "5m",
//    "15m",
    "30m",
//    "1h",
    "2h",
//    "4h",
//    "6h",
//    "12h",
//    "1d",
//    "1w",
};

const struct TPairData BNC_PairsData[] = {
    {.pair_name     = "BTCUSDT",
     .num_intervals = ITEMSOF(BNC_BTCUSDT_IntervalsData),
     .intervals_data = BNC_BTCUSDT_IntervalsData
    },
    {.pair_name     = "BNBUSDT",
     .num_intervals  = ITEMSOF(BNC_BNBUSDT_IntervalsData),
     .intervals_data = BNC_BNBUSDT_IntervalsData
    },
};

const struct TMarketData MarketsData[] = {
    // binance
    {.market_name   = "Binance",
     .num_pairs     = ITEMSOF(BNC_PairsData),
     .pairs_data    = BNC_PairsData,
     .max_bars_for_request = 500,
     .requests_freezytime = 1000},  // ms
    // poloniex
//    {},
    // other
//    {},

};

// "BTCUSDT" "1d" "12.05.2017 00:00:00" "12.05.2018 00:00:00"
string start_date = "01.06.2018 00:00:00";
string end_date = "15.06.2018 00:00:00";

//---------------------------------------------------------------------------
// входные данные:
// 1) торговая пара в виде "BTCUSDT", "BNBBTC"
// 2) интервал, для которого сливаем архив в виде "3m", "5m", "15m", "30m", "1h", "2h", "3h", "6h", "12h", "1d", "3d", "1w"
// 3) начальная дата с которой будем делать слив в виде "12.05.2018 00:33:00"
// 4) конечная дата до которой будем делать слив в виде "12.05.2018 00:33:00"
// 5) префикс имени выходного файла (основная инфа уже будет включена) (необязательный)
// 6) режим сохранения данных в файл - перезапись (создание нового файла) или дополнение существующего (необязательный)
int main(int argc, char *argv[])
{
    bool use_fname_prefix = false;

    cout << "==============================================" << endl;
//    cout << "check args " << argc << endl;
//    if (argc < 4) {
//        ShowMessageAndExit("Arguments count is too low.\nFormat: [symbol][interval][start date][end date][filename prefix][mode]",-1);
//    }

//    // PARSE INPUT PARAMETERS
//    string trading_pair = argv[1];
//    string interval = argv[2];
//    string start_date = argv[3];
//    string end_date = argv[4];
//    string fn_prefix = "";
//    if (argc >= 6) {
//        fn_prefix = argv[5];
//        use_fname_prefix = true;
//    }

    //=== инициализация binance api
    Json::Value result;
//    map<time_t, map<string, double>> klines_cache;

    string api_key 		= API_KEY;
    string secret_key 	= SECRET_KEY;
    BinaCPP::init( api_key , secret_key );

    // получение времени с биржи
    cout << "Get binance time" << endl;
    cout << KBLU;
    BinaCPP::get_serverTime( result );
    time_t binance_time = result["serverTime"].asInt64();
    cout << KGRN;
    cout << "binance time = " << binance_time << endl;
    cout << RESET;

    // расчет времени
    cout << "convert time" << endl;
    time_t end_timestamp = binance_time;    // конечное время = времени биржи
    time_t start_timestamp = GetTimeInUTC(start_date)*1000LL;
//    time_t end_timestamp = GetTimeInUTC(end_date)*1000LL;
    cout << "start_date: '" << start_date << "' -> " << start_timestamp << endl;
//    cout << "end_date: '" << end_date << "' -> " << end_timestamp << endl;

    if ( (start_timestamp == -1) || (end_timestamp == -1)) {
        cout << KRED;
        ShowMessageAndExit("Invalid start or end date", -1);
    }
    if(start_timestamp >= end_timestamp) {
        cout << KRED;
        ShowMessageAndExit("Start date should be more then end date", -1);
    }


    //=== инициализация sqlite
    using namespace sqlite_orm;
    int DB_record_type_remove = 0;

    //=== сохранение данных бирж в БД
    for(int mrkt_indx = 0; mrkt_indx < ITEMSOF(MarketsData); mrkt_indx++)
    {
        cout << "  Market name = " << MarketsData[mrkt_indx].market_name << endl;
        for(int npair = 0; npair < MarketsData[mrkt_indx].num_pairs; npair++)
        {
            string db_name = MarketsData[mrkt_indx].market_name + "-" + MarketsData[mrkt_indx].pairs_data[npair].pair_name + ".db";
            cout << "    Pair name = " << MarketsData[mrkt_indx].pairs_data[npair].pair_name << endl;
            for(int ninterval = 0; ninterval < MarketsData[mrkt_indx].pairs_data[npair].num_intervals; ninterval++)
            {
                string table_name = MarketsData[mrkt_indx].pairs_data[npair].pair_name
                                    + MarketsData[mrkt_indx].pairs_data[npair].intervals_data[ninterval];
                cout << "      Interval name = " << MarketsData[mrkt_indx].pairs_data[npair].intervals_data[ninterval] << endl;
                cout << KYEL;
                cout << "      DB name = " << db_name << "; Table name = " << table_name << endl;
                auto DBstorage = make_storage(db_name,
                                            make_table(table_name,
                                                       make_column("open_time",
                                                                   &CandlData::open_time),
                                                       make_column("close",
                                                                   &CandlData::close),
                                                       make_column("high",
                                                                   &CandlData::high),
                                                       make_column("low",
                                                                   &CandlData::low),
                                                       make_column("open",
                                                                   &CandlData::open),
                                                       make_column("volume",
                                                                   &CandlData::volume)));
                DBstorage.sync_schema();

                if(DB_record_type_remove)
                    DBstorage.remove_all<CandlData>();

                //=== определение последней записи в БД
                // инкремент бинансового времени на 1 бар запрашиваемого интервала
                auto interval = MarketsData[mrkt_indx].pairs_data[npair].intervals_data[ninterval];
                time_t ts_increment_1bar = 1000 * CalcTimeIncrement(interval);                
                cout << "      CalcTimeIncrement: " << " '" << interval << "' -> " << ts_increment_1bar/1000 << endl;

                // читаем из БД последнюю запись в таблице
//                auto str_last_open_time = DBstorage.max(&CandlData::open_time);
                time_t db_last_open_time;
                auto vctr_last_open_time = DBstorage.select(&CandlData::open_time, order_by(&CandlData::open_time).desc(), limit(1));
//                cout << "      vctr_last_open_time.size = " << vctr_last_open_time.size() << endl;
                if(vctr_last_open_time.size() == 1)
                    db_last_open_time = vctr_last_open_time[0];
                else if(vctr_last_open_time.size() == 0)
                    db_last_open_time = 0;  // записей в таблице нет
                else {
                    cout << KRED;
                    ShowMessageAndExit("Can't get last timestamp form DB. exit.", -1);
                }
                cout << "      Timestamp form the last record of DB = " << db_last_open_time << endl;
                if( (db_last_open_time + ts_increment_1bar) > end_timestamp ) {
                    // данные актуальные, читать ничего не надо
                    cout << KGRN;
                    cout << "      Skip this interval" << endl;
                    cout << RESET;
                    continue;
                };

                //=== чтение данных из биржи для конкретного интервала конкретной пары
                // начинаем чтение с последней записи в таблице + 1
                if(db_last_open_time >= start_timestamp)
                    start_timestamp = db_last_open_time + ts_increment_1bar;
                // бананс позволяет за 1 запрос слить не более 500 баров, поэтому
                // делаем цикл, в котором будем херачить запросы по 500 бар
                cout << RESET;
                cout << "      read from market cycle" << endl;
                cout << KYEL;
                cout << "      start_timestamp " << start_timestamp << "; end_timestamp " << end_timestamp << endl;
                for ( time_t ts = start_timestamp; ts <= end_timestamp; ts += (ts_increment_1bar * MarketsData[mrkt_indx].max_bars_for_request) ) {
                    time_t start_t = ts;
                    time_t end_t = ts + (ts_increment_1bar * MarketsData[mrkt_indx].max_bars_for_request);
                    if (end_t > end_timestamp)
                        end_t = end_timestamp;

                    auto trading_pair = MarketsData[mrkt_indx].pairs_data[npair].pair_name;
                    cout << RESET;
                    cout << "        get_klines: " << trading_pair << "; " << interval << "; " << "0; " << start_t << "; " << end_t << endl;
#if 1
                    // Get Klines/CandleStick
                    cout << KBLU;
                    BinaCPP::get_klines(trading_pair.c_str(), interval.c_str(), 0, start_t, end_t, result);

                    cout << RESET;
                    cout << "        write to DB" << endl;
                    for (unsigned int i = 0; i < result.size(); i++)
                        DBstorage.insert(CandlData{ result[i][0].asInt64(),
                                                    from_string<double>( result[i][1].asString().c_str() ),
                                                    from_string<double>( result[i][2].asString().c_str() ),
                                                    from_string<double>( result[i][3].asString().c_str() ),
                                                    from_string<double>( result[i][4].asString().c_str() ),
                                                    from_string<double>( result[i][5].asString().c_str() ),
                                                   } );
#endif
//                    for (int i = 0 ; i < result.size() ; i++) {
//                        time_t start_of_candle = result[i][0].asInt64();
//                        klines_cache[start_of_candle]["o"] = from_string<double>( result[i][1].asString().c_str() );
//                        klines_cache[start_of_candle]["h"] = from_string<double>( result[i][2].asString().c_str() );
//                        klines_cache[start_of_candle]["l"] = from_string<double>( result[i][3].asString().c_str() );
//                        klines_cache[start_of_candle]["c"] = from_string<double>( result[i][4].asString().c_str() );
//                        klines_cache[start_of_candle]["v"] = from_string<double>( result[i][5].asString().c_str() );
//                    }
                    cout << KGRN;
                    cout << "        writed successfuly" << endl;
                    cout << RESET;
                    // some delay between requests
                    QThread::msleep(MarketsData[mrkt_indx].requests_freezytime);
                }
                //=== чтение интервала завершено.
            }
        }
    }
    cout << RESET;

    //=== сохранение в файл (для отладки)
#if 0
//    cout << "Saving data to file..." << endl;    
    string filename = "";
    if(use_fname_prefix) {
        filename += fn_prefix;
        filename += string("_");
    }
    filename += trading_pair;
    filename += string("_");
    filename += interval;
    filename += string("_");
    filename += GetFixedDateTimeString(start_date);
    filename += string("_to_");
    filename += GetFixedDateTimeString(end_date);
    filename += string(".csv");
    SaveKlinesCache(filename, klines_cache);
//    cout << "DONE" << endl;

#endif
    ShowMessageAndExit("DONE", 0);
}
