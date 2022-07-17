#include "db.hpp"

string DB::cur_time() {
    char buf_time[20];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buf_time, sizeof(buf_time), "%Y-%m-%d %H:%M:%S", timeinfo);
    return string(buf_time);
}

void DB::insert_loop() {
    while (1) {
        try {
            this_thread::sleep_for(chrono::milliseconds(INSERT_PERIOD));

            int scnt = 0;
            string* args;
            string sql;

            m.lock();
            scnt = sql_queue.size();
            m.unlock();
            printf("%d..", scnt);
            if (scnt <= 0)
                continue;

            for (int sidx = 0; sidx < scnt; sidx++) {
                m.lock();
                args = sql_queue.front();
                sql_queue.pop();
                m.unlock();

                if (sql.length() > 0)
                    sql += ", ";
                sql += "('"
                    + args[0]
                    + "', '"
                    + args[1]
                    + "', '"
                    + args[2]
                    + "', '"
                    + args[3]
                    + "')";
            }
            sql = "INSERT INTO tmp_chat (`channel_name`, `user_name`, `chat_text`, `chat_time`) VALUES " + sql + ";";
            string sql_utf8 = boost::locale::conv::to_utf<char>(sql, "EUC-KR");
            if (mysql_query(conn_insert, sql_utf8.c_str())) {
                printf("SQL ERROR : %s\n", mysql_error(conn_insert));
                cout << sql << endl;
            }
            else {
                if (mysql_query(conn_insert, "CALL add_chat();")) {
                    printf("SQL ERROR : %s\n", mysql_error(conn_insert));
                }
            }
        }
        catch (int e) {
            printf("DB::insert_loop() ERROR (%d)\n", e);
            return;
        }
    }
}

DB::DB() {
}

DB::~DB() {
}

bool DB::connect(string host, string port, string user_id, string user_pw, string db_name) {
    mysql_init(&conn_opt_insert);
    mysql_init(&conn_opt_select);
    mysql_options(&conn_opt_insert, MYSQL_SET_CHARSET_NAME, "utf8");
    mysql_options(&conn_opt_insert, MYSQL_INIT_COMMAND, "SET NAMES utf8");
    mysql_options(&conn_opt_select, MYSQL_SET_CHARSET_NAME, "utf8");
    mysql_options(&conn_opt_select, MYSQL_INIT_COMMAND, "SET NAMES utf8");

    if (conn_insert = mysql_real_connect(&conn_opt_insert, host.c_str(), user_id.c_str(), user_pw.c_str(), db_name.c_str(), atoi(port.c_str()), (char*)NULL, 0)) {
        if (conn_select = mysql_real_connect(&conn_opt_select, host.c_str(), user_id.c_str(), user_pw.c_str(), db_name.c_str(), atoi(port.c_str()), (char*)NULL, 0)) {
            return true;
        }
        else {
            printf("DB CONNECT FAIL : %s\n", mysql_error(&conn_opt_select));
            return false;
        }
    }
    else {
        printf("DB CONNECT FAIL : %s\n", mysql_error(&conn_opt_insert));
        return false;
    }
}

void DB::insert(string channel, string user_name, string chat_text) {
    string time = cur_time();
    m.lock();
    sql_queue.push(new string[]{ channel, user_name, chat_text, time });
    m.unlock();
}

vector<string> DB::get_channels() {
    vector<string> channels;
    if (mysql_query(conn_select, "SELECT top_channel_name FROM `top_channel` ORDER BY top_channel_rank ASC LIMIT 100;") == 0) {
        MYSQL_RES* rows = mysql_store_result(conn_select);
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(rows)) != NULL) {
            channels.push_back(string(row[0]));
        }
        mysql_free_result(rows);
    }
    return channels;
}

void DB::start() {
    th = thread(&DB::insert_loop, this);
    th.detach();
}

void DB::join() {
    th.join();
}
