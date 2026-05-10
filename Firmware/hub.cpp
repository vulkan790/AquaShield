#define DEBUG_HUB   // раскомментировать для отладочного вывода

#include <cstdint>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <RCSwitch.h>
#include <Preferences.h>
#include <WebServer.h>
WebServer server(80);


// ============================ //
// PINS SECTION                 //
#define     p_rx            33
#define     p_tx_en         15
#define     p_tx            27
#define     p_btn_cfg       14
#define     p_led_red       5
#define     p_led_grn       16
#define     p_led_yel       17
// PINS SECTION                 //
// ============================ //

// ============================ //
// MESSAGES SECTION             //
#pragma region MESSAGES
#define     REQ         0
#define     RES         1

#pragma pack(push, 1)
struct st_snsr_tx_msg 
{
    uint16_t    id;
    uint8_t     type       : 1;
    uint8_t     _init      : 1;
    uint8_t     wtr        : 1;
    uint8_t     btr        : 1;
    uint8_t     r          : 4;
    uint8_t     crc;
};

struct st_snsr_rx_msg
{
    uint16_t    sid;
    uint8_t     data;
    uint8_t     num;
};

struct st_snsr_cfg_msg
{
    uint16_t    hid;
    uint16_t    sid;
    uint8_t     wtr_lvl     : 2;
    uint8_t     btr_lvl     : 2;
    uint8_t     mode_c      : 2;
    uint8_t     mode_a      : 2;
    uint16_t    dc;
    uint8_t     crc;
};

struct st_rly_msg
{
	uint16_t  rid;
    uint8_t   data;      // 1 = закрыть, 0 = открыть
    uint8_t   crc;
};

union msg_unit_s
{
    uint32_t raw;
    st_snsr_tx_msg stm;
    st_snsr_rx_msg srm;
    st_rly_msg rly;
};
#pragma pack(pop)
#pragma endregion MESSAGES
// MESSAGES SECTION             //
// ============================ //

// ============================ //
// OBJECTS                      //
RCSwitch o_rx;
RCSwitch o_tx;

Preferences prefs;

msg_unit_s rmsg;
msg_unit_s tmsg;
msg_unit_s lrmsg;
msg_unit_s ltmsg;
// OBJECTS                      //
// ============================ //

// ============================ //
// SENSORS                      //
#pragma region SNSRS

#define SENSOR_MAX   64

struct st_sensor_info
{
    uint16_t    id;
    bool        act;
    uint8_t     wtr_l;
    uint8_t     btr_l;
    uint8_t     mode_c;
    uint8_t     mode_a;
    uint16_t    dc;
    bool        blck;
    bool        wtr;
    bool        btr;
    long        seen;
};

st_sensor_info snsrs[SENSOR_MAX];
Preferences snsrs_prefs;

uint8_t snsrs_find(uint16_t id)
{
    for (int8_t i = 0; i < SENSOR_MAX; i++)
    {
        if (snsrs[i].act && snsrs[i].id == id)
        {
            return i;
        }
    }
    return SENSOR_MAX;
}

uint8_t snsrs_find_free()
{
    for (int8_t i = 0; i < SENSOR_MAX; i++)
    {
        if (!snsrs[i].act)
        {
            return i;
        }
    }
    return SENSOR_MAX;
}

void snsrs_save()
{
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Entering %s\n", __func__, millis(), __func__);
#endif
    snsrs_prefs.begin("ss", false);
    snsrs_prefs.putBytes("ss", snsrs, sizeof(snsrs));
    snsrs_prefs.end();

    String _debug_msg = "[snsrs_save] Saved to NVM " + String(sizeof(snsrs)) + " bytes";
    Serial.println(_debug_msg);
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Done %s\n", __func__, millis(), __func__);
#endif
}

void load_snsrs()
{
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Entering %s\n", __func__, millis(), __func__);
#endif
    memset(snsrs, 0, sizeof(snsrs));    
    snsrs_prefs.begin("ss", true);
    
    size_t len = snsrs_prefs.getBytesLength("ss");
    if (len == sizeof(snsrs))
    {
        snsrs_prefs.getBytes("ss", snsrs, sizeof(snsrs));
        
        int8_t _cnt = 0;
        for (int8_t i = 0; i < SENSOR_MAX; i++)
        {
            if (snsrs[i].act) _cnt++;            
        }
        Serial.printf("--[load_snsrs]: Loaded %d sensors", _cnt);        
    }
    else
    {
        Serial.println("--[load_snsrs]: No saved sensors!");
    }
    snsrs_prefs.end();
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Done %s\n", __func__, millis(), __func__);
#endif
}
#pragma endregion SNSRS
// SENSORS                      //
// ============================ //

// ============================ //
// RELAYS                       //
#pragma region RELAYS

#define RELAY_MAX   16

struct st_rly
{
    uint16_t id;
    bool stat;
    bool act;
};

st_rly rlys[RELAY_MAX];
Preferences rlys_prefs;

uint8_t rlys_find(uint16_t id)
{
    for (int8_t i = 0; i < RELAY_MAX; i++)
    {
        if (rlys[i].id == id && rlys[i].act)
        {
            return i;
        }
    }
    return SENSOR_MAX;
}

uint8_t rlys_find_free()
{
    for (int8_t i = 0; i < RELAY_MAX; i++)
    {
        if (!rlys[i].act)
        {
            return i;
        }
    }
    return SENSOR_MAX;
}

void rlys_save()
{
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Entering %s\n", __func__, millis(), __func__);
#endif
    rlys_prefs.begin("rs", false);
    rlys_prefs.putBytes("rs", rlys, sizeof(rlys));
    rlys_prefs.end();

    String _debug_msg = "[rlys_save] Saved to NVM " + String(sizeof(rlys)) + " bytes";
    Serial.println(_debug_msg);
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Done %s\n", __func__, millis(), __func__);
#endif
}

void load_rlys()
{
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Entering %s\n", __func__, millis(), __func__);
#endif
    memset(rlys, 0, sizeof(rlys));    
    rlys_prefs.begin("rs", true);
    
    size_t len = rlys_prefs.getBytesLength("rs");
    if (len == sizeof(rlys))
    {
        rlys_prefs.getBytes("rs", rlys, sizeof(rlys));
        
        int8_t _cnt = 0;
        for (int8_t i = 0; i < RELAY_MAX; i++)
        {
            if (rlys[i].act) _cnt++;            
        }
        Serial.printf("--[load_rlys]: Loaded %d relays\n", _cnt);
    }
    else
    {
        Serial.println("--[load_rlys]: No saved relays!");
    }
    rlys_prefs.end();
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Done %s\n", __func__, millis(), __func__);
#endif
}

#pragma endregion RELAYS
// RELAYS                      //
// ============================ //

const uint16_t port = 8000;
const uint16_t c_hid = 1;       // ID хаба
bool srvr_on = false;
String ip;

uint64_t btn_lst = 0;
bool btn_pressed = false;
const uint64_t btn_hld_t = 1000;

uint64_t last_rx_time = 0;
const int64_t max_save_time = 2500;

int16_t alrts = 0;
uint64_t srvr_chck_lst = 0;
const uint64_t srvr_chck_dt = 60000;

void _setup_rx()
{
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Entering %s\n", __func__, millis(), __func__);
#endif    
    o_rx.setProtocol(1);
    o_rx.setReceiveTolerance(75);
    o_rx.enableReceive(digitalPinToInterrupt(p_rx));
    
#ifdef DEBUG_HUB    
    Serial.printf("[-%s] %lu: Enabled receive on %s\n", __func__, millis(), String(p_rx));
    Serial.printf("[-%s] %lu: Done %s\n", __func__, millis(), __func__);
#endif
}

void _setup_tx()
{
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Entering %s\n", __func__, millis(), __func__);
#endif
    o_tx.setProtocol(5);
    o_tx.setRepeatTransmit(12);
    o_tx.enableTransmit(p_tx);
    o_tx.disableTransmit();
#ifdef DEBUG_HUB    
    Serial.printf("[-%s] %lu: Enabled transmit on %s\n", __func__, millis(), String(p_tx));
    Serial.printf("[-%s] %lu: Done %s\n", __func__, millis(), __func__);
#endif
}

void _wifi_con()
{
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Entering %s\n", __func__, millis(), __func__);
#endif
    if (WiFi.status() != WL_CONNECTED)
    {
        WiFi.begin();        

        uint8_t _att = 0;
        while (WiFi.status() != WL_CONNECTED && _att < 40)
        {
            delay(500);
            Serial.print(".");
            _att++;
        }

#ifdef DEBUG_HUB 
        if (WiFi.status() == WL_CONNECTED)
        {
            Serial.printf("[-%s] %lu: Succesfully done! IP: %s\n", __func__, millis(), WiFi.localIP().toString());           
        } 
        else
        {
            Serial.printf("[-%s] %lu: Not connected to WiFi!!!\n", __func__, millis());          
        }
#endif   
    }
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Done %s\n", __func__, millis(), __func__);
#endif
}

void setup()
{
    uint64_t _start = millis();

    Serial.begin(115200);
    delay(100);
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Entering %s\n", __func__, millis(), __func__);
#endif

    pinMode(p_btn_cfg, INPUT_PULLUP);
    pinMode(p_tx_en, OUTPUT);
    digitalWrite(p_tx_en, LOW);

    pinMode(p_led_red, OUTPUT);
    pinMode(p_led_yel, OUTPUT);
    pinMode(p_led_grn, OUTPUT);
    
    _setup_rx();
    _setup_tx();
    
    prefs.begin("hub", false);
    ip = prefs.getString("ip", "");
    Serial.println("-[setup] Saved IP: " + (ip.length() ? ip : "NONE"));

    load_snsrs();
    load_rlys();

    if (digitalRead(p_btn_cfg) == LOW || ip.length() == 0)
    {
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: INIT state %s\n", __func__, millis(), __func__);
#endif
        _init();
    }
    else
    {
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: CLASSIC state %s\n", __func__, millis(), __func__);
#endif
    }

    _wifi_con();      
    
    server.on("/", handleRoot);
    server.on("/alert", handleAlert);
    server.on("/relay", handleRelay);
    server.on("/simulate", handleSimulate);
    server.onNotFound(handleNotFound);
    server.begin();
    Serial.println("[hub] Web server started on port 80");
    
    Serial.printf("[-setup-] Done setup by %d\n", millis() - _start);
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Done %s\n", __func__, millis(), __func__);
#endif
}

void _init()
{
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Entering %s\n", __func__, millis(), __func__);
#endif
    WiFiManager o_wm;
    WiFiManagerParameter o_ip("server", "IP сервера", ip.c_str(), 40);
    o_wm.addParameter(&o_ip);
    
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Starting\n", __func__, millis());
#endif
    if (!o_wm.startConfigPortal("ASUH000000"))
    {
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Resetting\n", __func__, millis());
#endif
        delay(100);
        ESP.restart();
    }

    ip = String(o_ip.getValue());
    prefs.putString("ip", ip);
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: AquaShield Server-IP: %s\n", __func__, millis(), ip);
    Serial.printf("[-%s] %lu: Done %s\n", __func__, millis(), __func__);
#endif
}

bool _chck_con()
{
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Entering %s\n", __func__, millis(), __func__);
#endif
    if (WiFi.status() != WL_CONNECTED) 
    {
        Serial.println("&[_chck_con] NO WIFI!!!");
#ifdef DEBUG_HUB
        Serial.printf("[-%s] %lu: Done %s (false)\n", __func__, millis(), __func__);
#endif
        return false;
    }
    if (ip.length() == 0)
    {
        Serial.println("&[_chck_con] NO SERVER!!!");
#ifdef DEBUG_HUB
        Serial.printf("[-%s] %lu: Done %s (false)\n", __func__, millis(), __func__);
#endif
        return false;
    }
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Done %s (true)\n", __func__, millis(), __func__);
#endif
    return true;
}

void _parse_ans(HTTPClient &http)
{
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Entering %s\n", __func__, millis(), __func__);
#endif
    int32_t httpCode = http.GET();

    if (httpCode > 0)
    {
        Serial.println("-----[_parse_ans] Code: " + String(httpCode));
        if (httpCode == HTTP_CODE_OK)
        {
            String ans = http.getString();
            Serial.println("-----[_parse_ans] Payload: " + ans);
        }
    }
    else
    {
        Serial.println("-----[_parse_ans] Err: " + http.errorToString(httpCode));
    }
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Done %s\n", __func__, millis(), __func__);
#endif
}

void _alert_wtr(uint16_t sid, bool wtr)
{
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Entering %s\n", __func__, millis(), __func__);
#endif
    if (!_chck_con()) return;

    HTTPClient http;
    String url = "http://" + ip + ":" + String(port) + "/wtr/";
    url += wtr ? "leak" : "dry";
    url += "?hub_id=" + String(c_hid) + "&sensor_id=" + String(sid);

#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Sending HTTP alert...\n", __func__, millis());
    Serial.print(" Sending ALERT on " + url + " ... ");
#endif

    http.begin(url);
    _parse_ans(http);
    http.end();
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: HTTP alert sent: %s\n", __func__, millis(), wtr ? "leak" : "dry");
#endif
}

void _alert_btr(uint16_t sid, bool btr)
{
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Entering %s\n", __func__, millis(), __func__);
#endif
    if (!_chck_con()) return;

    HTTPClient http;
    String url = "http://" + ip + ":" + String(port) + "/btr/";
    url += btr ? "low" : "normal";
    url += "?hub_id=" + String(c_hid) + "&sensor_id=" + String(sid);

#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Sending HTTP battery alert...\n", __func__, millis());
    Serial.print("Sending ALERT on " + url + " ... ");
#endif

    http.begin(url);
    _parse_ans(http);
    http.end();
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: HTTP BTR sent: %s\n", __func__, millis(), btr ? "low" : "normal");
#endif
}

void _notify_rly(uint16_t rid, bool opened)
{
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Entering %s\n", __func__, millis(), __func__);
#endif
    if (!_chck_con()) return;

    HTTPClient http;
    String url = "http://" + ip + ":" + String(port) + "/relays/";
    url += opened ? "opened" : "blocked";
    url += "?hub_id=" + String(c_hid) + "&relay_id=" + String(rid);

#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Sending relay NOTIFY...\n", __func__, millis());
    Serial.print("Sending NOTIFY on " + url + " ... ");
#endif

    http.begin(url);
    _parse_ans(http);
    http.end();
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Relay notif sent: %s\n", __func__, millis(), opened ? "opened" : "blocked");
#endif
}

void _send_rly_cmd(uint16_t rid, bool close)
{
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Entering %s\n", __func__, millis(), __func__);
#endif
    st_rly_msg relayMsg;
    relayMsg.rid = rid;
    relayMsg.data = close ? 1 : 0;
    relayMsg.crc = 0;   // CRC сейчас не вычисляется, но поле есть

    msg_unit_s relayUnion;
    relayUnion.rly = relayMsg;

#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Sending relay RF cmd...\n", __func__, millis());
#endif
    digitalWrite(p_tx_en, HIGH);
    delay(5);
    o_tx.enableTransmit(p_tx);
    o_tx.send(relayUnion.raw, 32);
    o_tx.disableTransmit();
    digitalWrite(p_tx_en, LOW);

#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Sent relay cmd: %s, raw=0x%08lX\n", __func__, millis(), close ? "CLOSE" : "OPEN", relayUnion.raw);
    Serial.printf("[-%s] %lu: Done %s\n", __func__, millis(), __func__);
#endif
}

void _get_cfg()
{
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Entering %s\n", __func__, millis(), __func__);
#endif
    if (!_chck_con()) return;

    HTTPClient http;
    String url = "http://" + ip + ":" + String(port) + "/sensors/configuration";
    url += "?hub_id=" + String(c_hid);

#ifdef DEBUG_HUB
    Serial.println("--[_get_cfg] Getting CFG on " + url);
#endif    

    http.begin(url);
    int32_t httpCode = http.GET();

    if (httpCode > 0)
    {
        if (httpCode == HTTP_CODE_OK)
        {
            String payload = http.getString();
            Serial.println("Получен ответ: " + payload);

            DynamicJsonDocument doc(2048);
            DeserializationError error = deserializeJson(doc, payload);
            if (error)
            {
                Serial.print("Ошибка парсинга JSON: ");
                Serial.println(error.c_str());
                http.end();
                return;
            }

            JsonArray json_arr = doc["sensors"].as<JsonArray>();
            if (json_arr.isNull())
            {
                Serial.println("Ошибка: в JSON нет массива sensors");
                http.end();
                return;
            }

            int updatedCount = 0;
            for (JsonObject cur_snsr_cfg : json_arr)
            {
                int id = cur_snsr_cfg["id"].as<int>();
                int idx = snsrs_find(id);

                if (idx == SENSOR_MAX) 
                {
                    idx = snsrs_find_free();
#ifdef DEBUG_HUB
                Serial.printf("[-%s-] %lu: Created sensor %u\n", __func__, millis(), id);
#endif
                }

#ifdef DEBUG_HUB
                Serial.printf("[-%s-] %lu: Updating sensor %u config\n", __func__, millis(), id);
#endif
                snsrs[idx].mode_c = cur_snsr_cfg["work_mode"].as<int>();
                snsrs[idx].btr_l = cur_snsr_cfg["battery_threshold"].as<int>();
                snsrs[idx].wtr_l = cur_snsr_cfg["water_threshold"].as<int>();
                snsrs[idx].mode_a = cur_snsr_cfg["alert_mode"].as<int>();
                snsrs[idx].blck = cur_snsr_cfg["block_water"].as<bool>();

                
                updatedCount++;
                Serial.printf("Обновлён датчик ID %d: wtr_l=%d, btr_l=%d, mode_c=%d, mode_a=%d\n",
                                id, snsrs[idx].wtr_l, snsrs[idx].btr_l,
                                snsrs[idx].mode_c, snsrs[idx].mode_a);                
            }
            Serial.printf("Обновлено %d датчиков\n", updatedCount);
            snsrs_save();
        } else {
            Serial.printf("Ошибка HTTP: %d\n", httpCode);
        }
    } else {
        Serial.printf("Ошибка соединения: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Done %s\n", __func__, millis(), __func__);
#endif
}

void _send_msg(uint16_t sid, uint16_t data, uint8_t num)
{
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Sending cfg packet #%d to sensor %u\n", __func__, millis(), num, sid);
    Serial.printf("#%d: sid=0x%04X, data=0x%02X, num=0 => raw=0x%08lX\n", 
                  num, tmsg.srm.sid, tmsg.srm.data, (unsigned long)tmsg.raw);
#endif
    tmsg.srm.sid = sid;
    tmsg.srm.data = data;
    tmsg.srm.num = num;
    o_tx.send(tmsg.raw, 32);
    delay(20);
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Sent cfg packet #%d\n", __func__, millis(), num);
#endif
}

void _create_cfg_packets(const uint8_t idx, uint8_t& data0, uint8_t& data1, uint8_t& data2)
{
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Entering %s\n", __func__, millis(), __func__);
#endif
    st_sensor_info snsr = snsrs[idx];    
    
    uint8_t wtr_lvl = snsr.wtr_l;
    uint8_t btr_lvl = snsr.btr_l;
    uint8_t mode_c  = snsr.mode_c;
    uint8_t mode_a  = snsr.mode_a;

    uint16_t dc = snsr.dc;
    
    data0 = dc & 0xFF;
    data1 = (dc >> 8) & 0xFF;
    data2 = (wtr_lvl << 6) | (btr_lvl << 4) | (mode_c << 2) | mode_a; // биты: 7-6 wtr, 5-4 btr, 3-2 mode_c, 1-0 mode_a

#ifdef DEBUG_HUB
    Serial.println("[-_create_cfg_packets] Packets:");
    Serial.printf("  wtr_lvl=%d, btr_lvl=%d, mode_c=%d, mode_a=%d, dc=%d\n", wtr_lvl, btr_lvl, mode_c, mode_a, dc);
    Serial.printf("  data0 (мл. dc) = 0x%02X (%d)\n", data0, data0);
    Serial.printf("  data1 (ст. dc) = 0x%02X (%d)\n", data1, data1);
    Serial.printf("  data2 (params) = 0x%02X (bin: ");
    for (int i = 7; i >= 0; i--) Serial.print((data2 >> i) & 1);
    Serial.println(")");
    Serial.printf("[-%s] %lu: Done %s\n", __func__, millis(), __func__);
#endif
}

void _send_cfg(uint16_t sid)
{
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Entering %s\n", __func__, millis(), __func__);
#endif
    int idx = snsrs_find(sid);

    if (idx == SENSOR_MAX)
    {
        Serial.println("--[_send_cfg] Unsupported SID: " + String(sid));
#ifdef DEBUG_HUB
        Serial.printf("[-%s] %lu: Done %s (not found)\n", __func__, millis(), __func__);
#endif
        return;
    }

    uint8_t data0, data1, data2;
    _create_cfg_packets(idx, data0, data1, data2);
    
    digitalWrite(p_tx_en, HIGH);
    delay(10);
    o_tx.enableTransmit(p_tx);
        
    _send_msg(sid, data0, 0);
    _send_msg(sid, data1, 1);
    _send_msg(sid, data2, 2);   

    o_tx.disableTransmit();
    digitalWrite(p_tx_en, LOW);    
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Done %s\n", __func__, millis(), __func__);
#endif
}

void _btn_lgc()
{
#ifdef DEBUG_HUB
    //Serial.printf("[-%s] %lu: Entering %s\n", __func__, millis(), __func__);
#endif
    int state = digitalRead(p_btn_cfg);

    if (state == LOW)
    {
        if (!btn_pressed)
        {
            btn_pressed = true;
            btn_lst = millis();
#ifdef DEBUG_HUB
            Serial.printf("[-%s] %lu: Button pressed\n", __func__, millis());
#endif
        }
    }
    else
    {
        if (btn_pressed)
        {
            btn_pressed = false;
            unsigned long held = millis() - btn_lst;
#ifdef DEBUG_HUB
            Serial.printf("[-%s] %lu: No press %lu ms\n", __func__, millis(), held);
#endif
            if (held < btn_hld_t) 
            {
#ifdef DEBUG_HUB
                Serial.printf("[-%s] %lu: Short press\n", __func__, millis());
#endif
                if (_chck_con())
                {
#ifdef DEBUG_HUB
                Serial.printf("[-%s] %lu: Server OK, requesting...\n", __func__, millis());
#endif
                    _get_cfg();
                } 
            }
        }
    }
#ifdef DEBUG_HUB
    //Serial.printf("[-%s] %lu: Done %s\n", __func__, millis(), __func__);
#endif
}

void _updt_leds()
{
#ifdef DEBUG_HUB
    // Serial.printf("[-%s] %lu: Entering %s\n", __func__, millis(), __func__);
#endif
    digitalWrite(p_led_grn, HIGH);      // всегда горит зелёный (питание)
    digitalWrite(p_led_red, alrts > 0 ? HIGH : LOW);

    bool no_wifi = (WiFi.status() != WL_CONNECTED);
    bool no_server = (ip.length() > 0 && !srvr_on);
    digitalWrite(p_led_yel, (no_wifi || no_server) ? HIGH : LOW);
#ifdef DEBUG_HUB
    // Serial.printf("[-%s] %lu: Done %s (alerts=%d, wifi=%d, server=%d)\n", __func__, millis(), __func__, alrts, !no_wifi, !no_server);
#endif
}

bool _ping()
{
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Pinging server...\n", __func__, millis());
#endif
    srvr_chck_lst = millis();
    if (!_chck_con()) return false;

    HTTPClient http;
    http.setTimeout(2000);
    http.begin("http://" + ip + ":" + String(port) + "/ping");

    int code = http.GET();
    http.end();

    bool ok = (code == HTTP_CODE_OK);
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Ping result: %s\n", __func__, millis(), ok ? "OK" : "FAIL");
#endif
    return ok;
}

void _handle_snsr_msg(int idx)
{
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Entering with idx %d (id=%u)\n", __func__, millis(), idx, rmsg.stm.id);
#endif
    if (rmsg.stm.wtr && rmsg.stm.type == REQ)
    {
#ifdef DEBUG_HUB
        Serial.printf("[-%s] %lu: WTR ALERT %u\n", __func__, millis(), rmsg.stm.id);
#endif
        snsrs[idx].wtr = true;                        
        _alert_wtr(rmsg.stm.id, true);
        alrts += 1;
#ifdef DEBUG_HUB
        Serial.printf("[-%s] %lu: alerts++: %d\n", __func__, millis(), alrts);
#endif
        if (snsrs[idx].blck)
        {
#ifdef DEBUG_HUB
            Serial.printf("[-%s] %lu: closing relays\n", __func__, millis());
#endif
            for (int i = 0; i < RELAY_MAX; i++)
            {
                if (rlys[i] != 0)
                {
                    _send_rly_cmd(rlys[i], true);
                    _notify_rly(rlys[i], false);
                }
            }
        }
    }
    else if (rmsg.stm.wtr && rmsg.stm.type == RES)
    {
#ifdef DEBUG_HUB
        Serial.printf("[-%s] %lu: WTR OK %u\n", __func__, millis(), rmsg.stm.id);
#endif
        snsrs[idx].wtr = false;
        _alert_wtr(rmsg.stm.id, false);
        alrts -= 1;
#ifdef DEBUG_HUB
        Serial.printf("[-%s] %lu: alerts--:  %d\n", __func__, millis(), alrts);
#endif
        bool any_block = false;
        for (int i = 0; i < SENSOR_MAX; i++)
        {
            if (snsrs[i].act && snsrs[i].blck && snsrs[i].wtr)
            {
                any_block = true;
                break;
            }
        }
        if (!any_block)
        {
#ifdef DEBUG_HUB
            Serial.printf("[-%s] %lu: Leaks = 0, open all\n", __func__, millis());
#endif
            for (int i = 0; i < RELAY_MAX; i++)
            {
                if (rlys[i] != 0)
                {
                    _send_rly_cmd(rlys[i], false);
                    _notify_rly(rlys[i], true);
                }
            }
        }
    }
    else if (rmsg.stm.btr && rmsg.stm.type == REQ)
    {
#ifdef DEBUG_HUB
        Serial.printf("[-%s-] %lu: BTR ALERT %u\n", __func__, millis(), rmsg.stm.id);
#endif
        snsrs[idx].btr = true;
        _alert_btr(rmsg.stm.id, true);
        alrts += 1;
#ifdef DEBUG_HUB
        Serial.printf("[-%s-] %lu: alerts++: %d\n", __func__, millis(), alrts);
#endif
    }
    else if (rmsg.stm.btr && rmsg.stm.type == RES)
    {
#ifdef DEBUG_HUB
        Serial.printf("[-%s-] %lu: BTR OK %u\n", __func__, millis(), rmsg.stm.id);
#endif
        snsrs[idx].btr = false;
        _alert_btr(rmsg.stm.id, true);   // ??? Note: original code sends "true" even on RES, might be a bug
        alrts -= 1;
#ifdef DEBUG_HUB
        Serial.printf("[-%s-] %lu: alerts--: %d\n", __func__, millis(), alrts);
#endif
    }
    else if (rmsg.stm._init)
    {
        if (rmsg.stm.type == REQ)
        {
#ifdef DEBUG_HUB
            Serial.printf("[-%s-] %lu: INIT REQ from sensor %u\n", __func__, millis(), rmsg.stm.id);
#endif
            _send_cfg(rmsg.stm.id);
        }
        else 
        {
#ifdef DEBUG_HUB
            Serial.printf("[-%s] %lu: INIT RES from sensor %u\n", __func__, millis(), rmsg.stm.id);
#endif
            snsrs[idx].seen = millis();                    
        }
    }
    else if (rmsg.stm.type == REQ && !rmsg.stm.btr)
    {
#ifdef DEBUG_HUB
        Serial.printf("[-%s] %lu: CFG REQ from sensor %u\n", __func__, millis(), rmsg.stm.id);
#endif
        _send_cfg(rmsg.stm.id);
    }
#ifdef DEBUG_HUB
    Serial.printf("[-%s] %lu: Done handling sensor msg\n", __func__, millis());
#endif
}

// ============ Встроенный веб-сервер ============


void handleRoot() {
#ifdef DEBUG_HUB
    Serial.printf("[%s] %lu: Entering ROOT web request\n", __func__, millis());
#endif
  String html = "<html><head><title>AquaShield Hub</title></head><body>";
  html += "<h1>Хаб умной защиты от протечек</h1>";
  html += "<p>IP хаба: " + WiFi.localIP().toString() + "</p>";
  html += "<p>Активных датчиков: ";
  int cnt = 0;
  for (int i = 0; i < SENSOR_MAX; i++) if (snsrs[i].act) cnt++;
  html += String(cnt) + "</p>";
  html += "<p>Тревог: " + String(alrts) + "</p>";
  html += "<p>Сервер доступен: " + String(srvr_on ? "Да" : "Нет") + "</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
#ifdef DEBUG_HUB
  Serial.printf("[%s] %lu: Root response sent\n", __func__, millis());
#endif
}

void handleAlert() {
#ifdef DEBUG_HUB
  Serial.printf("[%s] %lu: Entering ALERT web request\n", __func__, millis());
#endif
  if (server.hasArg("message")) {
    String msg = server.arg("message");
    Serial.printf("[web] Получено сообщение: %s\n", msg.c_str());
    digitalWrite(p_led_red, HIGH);
    delay(2000);
    digitalWrite(p_led_red, LOW);
    server.send(200, "text/plain", "Тревога обработана: " + msg);
#ifdef DEBUG_HUB
    Serial.printf("[%s] %lu: Alert processed\n", __func__, millis());
#endif
  } else {
    server.send(400, "text/plain", "Не указан параметр message");
#ifdef DEBUG_HUB
    Serial.printf("[%s] %lu: Missing message parameter\n", __func__, millis());
#endif
  }
}

void handleRelay() {
#ifdef DEBUG_HUB
  Serial.printf("[%s] %lu: Entering RELAY web request\n", __func__, millis());
#endif
  if (server.hasArg("action") && server.hasArg("id")) {
    String action = server.arg("action");
    int rid = server.arg("id").toInt();
    if (action == "close") {
      _send_rly_cmd(rid, true);
      server.send(200, "text/plain", "Реле закрыто");
    } else if (action == "open") {
      _send_rly_cmd(rid, false);
      server.send(200, "text/plain", "Реле открыто");
    } else {
      server.send(400, "text/plain", "Неверное действие");
    }
  } else {
    server.send(400, "text/plain", "Укажите action=close/open и id=...");
  }
#ifdef DEBUG_HUB
  Serial.printf("[%s] %lu: Relay command processed\n", __func__, millis());
#endif
}

void handleSimulate() {
#ifdef DEBUG_SENSOR
    Serial.printf("[-%s] %lu: Simulation request received\n", __func__, millis());
#endif
    // Проверяем наличие параметров sensor_id и event
    if (!server.hasArg("sensor_id") || !server.hasArg("event")) {
        server.send(400, "text/plain", "Missing sensor_id or event");
        return;
    }

    uint16_t sid = server.arg("sensor_id").toInt();
    String event = server.arg("event");
    event.toLowerCase();

    // Ищем датчик в таблице
    int idx = snsrs_find(sid);
    if (idx == SENSOR_MAX) {
        server.send(404, "text/plain", "Sensor not found");
        return;
    }

    // Формируем виртуальное сообщение
    msg_unit_s sim_msg;
    sim_msg.raw = 0;
    sim_msg.stm.id = sid;
    sim_msg.stm.type = REQ;        // по умолчанию REQ
    sim_msg.stm._init = 0;
    sim_msg.stm.wtr = 0;
    sim_msg.stm.btr = 0;

    if (event == "leak") {
        sim_msg.stm.wtr = 1;
    } else if (event == "dry") {
        sim_msg.stm.type = RES;    // сброс тревоги
        sim_msg.stm.wtr = 1;       // RES означает "протечки больше нет"
    } else if (event == "battery_low") {
        sim_msg.stm.btr = 1;
    } else if (event == "battery_normal") {
        sim_msg.stm.type = RES;
        sim_msg.stm.btr = 1;
    } else if (event == "init_req") {
        sim_msg.stm._init = 1;
        sim_msg.stm.type = REQ;
    } else if (event == "init_res") {
        sim_msg.stm._init = 1;
        sim_msg.stm.type = RES;
    } else if (event == "cfg_req") {
        sim_msg.stm._init = 1;
        sim_msg.stm.type = RES;
    } else if (event == "cfg_res") {
        sim_msg.stm._init = 1;
        sim_msg.stm.type = RES;
    }  
    else {
        server.send(400, "text/plain", "Unknown event. Use: leak, dry, battery_low, battery_normal, init_req, init_res");
        return;
    }

    // Сохраняем оригинальный rmsg и подменяем на эмулируемый
    msg_unit_s orig_rmsg = rmsg;
    rmsg = sim_msg;

    // Вызываем штатную обработку (она использует глобальный rmsg)
    _handle_snsr_msg(idx);
    snsrs_save();

    // Восстанавливаем оригинальное сообщение (на случай, если loop() уже обрабатывал что-то)
    rmsg = orig_rmsg;

    String response = "Simulated event '" + event + "' for sensor " + String(sid);
    server.send(200, "text/plain", response);

#ifdef DEBUG_SENSOR
    Serial.printf("[-%s] %lu: Simulation processed: %s\n", __func__, millis(), response.c_str());
#endif
}

void handleNotFound() {
#ifdef DEBUG_HUB
  Serial.printf("[%s] %lu: INVALID web request\n", __func__, millis());
#endif
  server.send(404, "text/plain", "Not found");
}

void loop()
{
#ifdef DEBUG_HUB
    //Serial.printf("[%s] %lu: Loop iteration\n", __func__, millis());
#endif
    _updt_leds();

    _btn_lgc();

    server.handleClient();

    if (millis() - last_rx_time > max_save_time)
    {
        lrmsg.raw = 0;
        last_rx_time = millis();
#ifdef DEBUG_HUB
        Serial.printf("[%s] %lu: CLEARED last received message\n", __func__, millis());
#endif
    }

    if (millis() - srvr_chck_lst > srvr_chck_dt)
    {
#ifdef DEBUG_HUB
        Serial.printf("[%s] %lu: Periodic server check\n", __func__, millis());
#endif
        srvr_on = _ping();        
    }

#ifdef DEBUG_HUB            
    if (Serial.available())
    {
        char c = Serial.read();
        if (c == 'l' || c == 'L')
        {
            Serial.println("\n--- SENSORS LIST ---");
            int cnt = 0;
            for (int i = 0; i < SENSOR_MAX; i++)
            {
                if (snsrs[i].act)
                {
                    Serial.printf("Idx %2d: ID=%5d, leak=%d, bat=%d, seen=%lu, wtr_l=%d, btr_l=%d, mode_c=%d, mode_a=%d, dc=%d\n",
                                  i, snsrs[i].id, snsrs[i].wtr, snsrs[i].btr,
                                  snsrs[i].seen, snsrs[i].wtr_l, snsrs[i].btr_l,
                                  snsrs[i].mode_c, snsrs[i].mode_a, snsrs[i].dc);
                    cnt++;
                }
            }
            Serial.printf("Total active sensors: %d\n", cnt);
            Serial.println("------------------------");
        }
    }
#endif 

    if (o_rx.available())
    {
#ifdef DEBUG_HUB
        Serial.printf("[%s] %lu: Msg recived\n", __func__, millis());
#endif
        rmsg.raw = o_rx.getReceivedValue();    

        if (rmsg.raw == lrmsg.raw)
            return;
        if (rmsg.raw == ltmsg.raw)
            return;

        lrmsg.raw = rmsg.raw;

#ifdef DEBUG_HUB
        Serial.printf("[%s] %lu: Processing msg  0x%08lX\n", __func__, millis(), rmsg.raw);
#endif
        int idx = snsrs_find(rmsg.stm.id);
        if (idx != SENSOR_MAX)
        {
#ifdef DEBUG_HUB
            Serial.printf("[%s] %lu: Sensor msg: from %u", __func__, millis(), rmsg.stm.id);
#endif
            snsrs[idx].seen = millis();
            _handle_snsr_msg(idx);
            snsrs_save();
        }      

        idx = rlys_find(rmsg.stm.id); 
        if (idx != SENSOR_MAX)
        {
            bool opened = rmsg.stm.wtr; // TODO: correct getting wtr
#ifdef DEBUG_HUB
            Serial.printf("[%s] %lu: Relay msg: from %u, opened=%d\n", __func__, millis(), rmsg.stm.id, opened);
#endif
            _notify_rly(rmsg.stm.id, opened);
        }                  

        last_rx_time = millis();
        o_rx.resetAvailable();
    }
#ifdef DEBUG_HUB
    //Serial.printf("[%s] %lu: End iteration\n", __func__, millis());
#endif
}