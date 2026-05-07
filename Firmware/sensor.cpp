#include "RCSwitch.h"
#include <cstdint>

// ============================ //
// PINS SECTION                 //
#define   p_led       2
#define   p_bzr       4

#define   p_btr       35
#define   p_wtr       34

#define   p_tx        15
#define   p_tx_en     14
#define   p_rx        5

#define   p_init_btn  23
// PINS SECTION                 //
// ============================ //


// ============================ //
// MESSAGES SECTION             //
#define AQSH_MSG_REQ 0
#define AQSH_MSG_RES 1

#pragma pack(push, 1)
struct st_snsr_tx_msg
{
    uint16_t  id;
    uint8_t   type	: 1;
    uint8_t   init	: 1;
    uint8_t   wtr	: 1;
    uint8_t   btr	: 1;
    uint8_t   r		: 4;
    uint8_t   crc;
};

struct st_snsr_rx_msg
{
    uint16_t  sid;
    uint8_t   data;
    uint8_t   num;
};

struct st_snsr_cfg_msg
{
    uint16_t        hid;
    uint16_t        sid;
    uint8_t         wtr_lvl	: 2;
    uint8_t         btr_lvl	: 2;
    uint8_t         mode_w	: 2;
    uint8_t         mode_a	: 2;
    uint16_t        dc;
    uint8_t         crc;
};

union msg_unit_s
{
    uint32_t            val;
    st_snsr_tx_msg      stm;
    st_snsr_rx_msg      srm;
};

union msg_unit_b
{
    uint64_t            val;
    st_snsr_cfg_msg     cm;
};

uint8_t cm_pos = 0; 
#pragma pack(pop)
// MESSAGES SECTION             //
// ============================ //


// ============================ //
// CONFIG TEMPLATES             //
#define ADC_MAX     4095
#define REF_VOLTAGE 3.3

// mode_w-fields
enum MODE_W { FULLPROOF, SAFE, CLASSIC, SAVING };
/*
                                                            SAVING
                                                    CLASSIC
                                                SAFE
                                    FULLPROOF
*/                                
const uint16_t c_lstn_wndws[]   =   { 7500,     7500,   5000,   3000    };
const uint16_t c_sleep_times[]  =   { 3000,     5000,   8000,   10000   };
const uint16_t c_cycles[]       =   { 600,      3600,   3600,   10800   };

// mode_a-fields
const uint16_t c_alert_pings[]  =   { 100,      3,      5,      1       };

const uint16_t c_wtr_trshlds[]  =   { 200,      500,    1000,   2000    };
const uint16_t c_btr_trshlds[]  =   { 33,       37,     42,     45      };
// CONFIG TEMPLATES             //
// ============================ //


// ============================ //
// OBJECTS                      //
uint16_t                freq                = 2000;

RCSwitch                o_tx                = RCSwitch();
RCSwitch                o_rx                = RCSwitch();

msg_unit_s              smsg;
msg_unit_b              bmsg;
// OBJECTS                      //
// ============================ //


// ============================ //
// RAM
const uint16_t          c_id                = 200;
RTC_DATA_ATTR uint16_t  c_hid               = 0;

RTC_DATA_ATTR uint16_t  c_wtr_trshld        = 500;
RTC_DATA_ATTR uint16_t  ulp_wtr             = 0;
RTC_DATA_ATTR uint16_t  c_btr_trshld        = 33;
RTC_DATA_ATTR uint16_t  c_lstn_wndw         = 10000;
RTC_DATA_ATTR uint16_t  c_sleep_time        = 1000;
RTC_DATA_ATTR uint16_t  c_cycle             = 20000;
RTC_DATA_ATTR uint16_t  c_alert_ping        = 1;
RTC_DATA_ATTR uint16_t  c_alert_ping_left   = 1;
RTC_DATA_ATTR int64_t   delta_time          = 0;
RTC_DATA_ATTR bool      loud_alarm          = true;

RTC_DATA_ATTR bool      alarm_mode          = false;
RTC_DATA_ATTR bool      low_btr             = false;
// RAM
// ============================ //


void setup_pins();
void _setup_rx();
void _slp(uint16_t);
bool _chck_wtr();
void _init();
void _send(bool, bool, bool, bool);
void _alarm_lgc();
void _start_cfg();
void _calc_crc();


void setup()
{  
#ifdef DEBUG_SENSOR
    int64_t _temp_time1;
    int64_t _temp_time2;
    int64_t start_time = millis();
    unsigned long current_time;

    current_time = millis();
    _temp_time1 = millis();
#endif

    setup_pins();

#ifdef DEBUG_SENSOR
    _temp_time2 = millis();
    Serial.printf("[setup] %lums: Done SETUP by %lld milliseconds\n", 
                current_time, _temp_time2 - _temp_time1);
#endif

#ifdef DEBUG_SENSOR
    current_time = millis();
    _temp_time1 = millis();
#endif

    _alarm_lgc();

#ifdef DEBUG_SENSOR
    _temp_time2 = millis();
    Serial.printf("[setup] %lums: Done ALARM_LOGIC by %lld milliseconds\n", 
                current_time, _temp_time2 - _temp_time1);
#endif

#ifdef DEBUG_SENSOR
    current_time = millis();
    _temp_time1 = millis();
#endif

    if (digitalRead(p_init_btn) == HIGH)
    {
        bool _is_init = _chs_rcv_mode();
        
        if (_is_init)
        {
#ifdef DEBUG_SENSOR
            Serial.printf("[setup] %lums: Entering INIT mode\n", millis());
#endif
            _init();
        }
        else
        {
#ifdef DEBUG_SENSOR
            Serial.printf("[setup] %lums: Entering CONFIG mode\n", millis());
#endif
            _start_cfg();
        }
    }
    else if (delta_time <= 0)
    {
#ifdef DEBUG_SENSOR
        Serial.printf("[setup] %lums: delta_time <= 0, entering CONFIG mode\n", millis());
#endif
        _start_cfg();
    }

#ifdef DEBUG_SENSOR
    _temp_time2 = millis();
    Serial.printf("[setup] %lums: Done RECEIVE_INITCFG by %lld milliseconds\n", 
                current_time, _temp_time2 - _temp_time1);
    
    // Замер времени выполнения функций (микросекунды)
    _temp_time1 = micros();
    delay(1);  // заглушка вместо реального кода
    _temp_time2 = micros();
    Serial.printf("[setup] %lums: Done timing check by %lld microseconds\n", 
                millis(), _temp_time2 - _temp_time1);
#endif

    delta_time = delta_time - (end_time - start_time + 2) - c_sleep_time;
    int64_t end_time = millis();
#ifdef DEBUG_SENSOR
    int64_t end_time = millis();
    Serial.printf("[setup] %lums: delta_time = %lld\n", end_time, delta_time);
#endif

    _slp(c_sleep_time);
}


void setup_pins()
{
#ifdef DEBUG_SENSOR
    Serial.printf("[-setup_pins] %lumcs: Entering\n", micros());
    Serial.begin(115200);
    Serial.printf("[-setup_pins] %lumcs: Done Serial.begin\n", micros());
#endif

    pinMode(p_led,        OUTPUT);
    pinMode(p_bzr,        OUTPUT);
    pinMode(p_tx_en,      OUTPUT);

    pinMode(p_wtr,        INPUT);
    pinMode(p_init_btn,   INPUT);
    pinMode(p_btr,        INPUT);

#ifdef DEBUG_SENSOR        
    Serial.printf("[-setup_pins] %lumcs: Done\n", micros());
#endif
}

void _setup_rx()
{
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_setup_rx] %lumcs: Entering\n", micros());
#endif
    o_rx.setProtocol(5);
    o_rx.setReceiveTolerance(60);
    o_rx.enableReceive(digitalPinToInterrupt(p_rx));
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_setup_rx] %lumcs: Done\n", micros());
#endif
}

bool _chs_rcv_mode()
{
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_chs_rcv_mode] %lums: Entering\n", millis());
#endif
    digitalWrite(p_led, HIGH);
    delay(100);
    for (int8_t i = 0; i < 4; i++)
    {
        digitalWrite(p_led, LOW);
        delay(900);
        digitalWrite(p_led, HIGH);
        delay(100);

        if (digitalRead(p_init_btn) == LOW)
        {
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_chs_rcv_mode] %lums: Done, TRUE\n", millis());
#endif
            return false;
        }
    }
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_chs_rcv_mode] %lums: Done, FALSE\n", millis());
#endif
    return true;
}

void _init()
{
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_init] %lumcs: Entering INIT\n", micros());
#endif    
    _send(AQSH_MSG_REQ, 1,0,0);
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_init] %lumcs: Sent INIT-msg\n", micros());
#endif    

    uint64_t start_time = millis();  
    uint64_t blink_time = millis();  
    bool _rec = false;
    bool _led_on = false;    

    _setup_rx();
    while (millis() < start_time + c_lstn_wndw && !_rec)
    {  
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_init] %lumcs: Available?\n", micros());
#endif  
        if (o_rx.available())
        {  
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_init] %lumcs: Available.\n", micros());
#endif  
            smsg.val = o_rx.getReceivedValue();
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_init] %lumcs: Entering INIT\n", micros());
    _debug_msg = String(200) + ": Received message: [" + String(smsg.val, HEX) + "]";
    Serial.println(_debug_msg);  
#endif  
            if (smsg.srm.sid == c_id)
            {
#ifdef DEBUG_SENSOR        
    _debug_msg = String(201) + ": Received message with correct SID: [" + String(smsg.val, HEX) + "]";
    Serial.println(_debug_msg);
#endif        
                if (smsg.srm.num == cm_pos)
                {
#ifdef DEBUG_SENSOR        
    _debug_msg = String(202) + ": Init messagge #" + String(cm_pos) + "[" + String(smsg.val, HEX) + "]";
    Serial.println(_debug_msg);
#endif 
                    bsmg.val |= smsg.srm.data;
                    cm_pos++;
                    bsmg.val <<= 1;
                }
            }

            if (cm_pos == 3)
            {
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_init] %lumcs: Received INIT\n", micros());
#endif 
                _rec = true;
            }

            o_rx.resetAvailable();
        }

        if (millis() > blink_time + 100 + 300 * (1 - _led_on))
        {
            blink_time = millis();
            digitalWrite(p_led, (_led_on ? HIGH : LOW));
        }
    }

    if (_rec)
    {
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_init] %lumcs: Setting cfg\n", micros());
#endif 
        digitalWrite(p_led, HIGH);
        delay(2000);
        digitalWrite(p_led, LOW);
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_init] %lumcs: Sending DONE INIT\n", micros());
#endif 
        _send(AQSH_MSG_RES, 1,0,0); 
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_init] %lumcs: Sent\n", micros());
#endif 

        delta_time = bmsg.cm.dc * 1000;
        _set_cfg();
    }
    else
    {
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_init] %lumcs: Cfg not found\n", micros());
#endif 
        for (int8_t i = 0; i < 10; i++)
        {    
            digitalWrite(p_led, HIGH);
            delay(50);
            digitalWrite(p_led, LOW);
            delay(50);
        }
    }

#ifdef DEBUG_SENSOR        
    Serial.printf("[-_init] %lumcs: Done\n", micros());
#endif 
}

void _start_cfg()
{
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_start_cfg] %lumcs: Entering CFG\n", micros());
#endif    
    _send(AQSH_MSG_REQ, 0,0,0);
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_start_cfg] %lumcs: Sent CFG-msg\n", micros());
#endif    

    uint64_t start_time = millis();  
    uint64_t blink_time = millis();  
    bool _rec = false;
    bool _led_on = false;    

    _setup_rx();
    while (millis() < start_time + c_lstn_wndw && !_rec)
    {  
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_start_cfg] %lumcs: Available?\n", micros());
#endif  
        if (o_rx.available())
        {  
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_start_cfg] %lumcs: Available.\n", micros());
#endif  
            smsg.val = o_rx.getReceivedValue();
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_start_cfg] %lumcs: Entering INIT\n", micros());
    _debug_msg = String(200) + ": Received message: [" + String(smsg.val, HEX) + "]";
    Serial.println(_debug_msg);  
#endif  
            if (smsg.srm.sid == c_id)
            {
#ifdef DEBUG_SENSOR        
    _debug_msg = String(201) + ": Received message with correct SID: [" + String(smsg.val, HEX) + "]";
    Serial.println(_debug_msg);
#endif        
                if (smsg.srm.num == cm_pos)
                {
#ifdef DEBUG_SENSOR        
    _debug_msg = String(202) + ": Init messagge #" + String(cm_pos) + "[" + String(smsg.val, HEX) + "]";
    Serial.println(_debug_msg);
#endif 
                    bsmg.val |= smsg.srm.data;
                    cm_pos++;
                    bsmg.val <<= 1;
                }
            }

            if (cm_pos == 3)
            {
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_start_cfg] %lumcs: Received INIT\n", micros());
#endif 
                _rec = true;
            }

            o_rx.resetAvailable();
        }

        if (millis() > blink_time + 100 + 300 * (1 - _led_on))
        {
            blink_time = millis();
            digitalWrite(p_led, (_led_on ? HIGH : LOW));
        }
    }

    if (_rec)
    {
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_start_cfg] %lumcs: Setting cfg\n", micros());
#endif 
        digitalWrite(p_led, HIGH);
        delay(2000);
        digitalWrite(p_led, LOW);
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_start_cfg] %lumcs: Sending DONE INIT\n", micros());
#endif 
        _send(AQSH_MSG_RES, 1,0,0); 
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_start_cfg] %lumcs: Sent\n", micros());
#endif 
        delta_time = bmsg.cm.dc;
        _set_cfg();
    }
    else
    {
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_start_cfg] %lumcs: Cfg not found\n", micros());
#endif 
        for (int8_t i = 0; i < 10; i++)
        {    
            digitalWrite(p_led, HIGH);
            delay(50);
            digitalWrite(p_led, LOW);
            delay(50);
        }
    }

#ifdef DEBUG_SENSOR        
    Serial.printf("[-_init] %lumcs: Done\n", micros());
#endif 
}

void _set_cfg()
{
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_set_cfg] %lumcs: Entering\n", micros());
#endif 
    c_wtr_trshld = c_wtr_trshlds[bmsg.cm.wtr_lvl];
    c_btr_trshld = c_btr_trshlds[bmsg.cm.btr_lvl];

    c_alert_ping_left = c_alert_ping;
    
    c_lstn_wndw = c_lstn_wndws[bmsg.cm.mode_w];
    c_cycle = c_cycles[bmsg.cm.mode_w] * 1000;
    c_sleep_time = c_sleep_times[bmsg.cm.mode_w];
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_set_cfg] %lumcs: Done\n", micros());
#endif 
}

void _alarm_lgc()
{
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_alarm_lgc] %lumcs: Entering\n", micros());
#endif
    bool wtr = _chck_wtr();
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_alarm_lgc] %lumcs: Checked water: %s\n", micros(), (wtr == 1 ? 
        "WET" : "DRY"));
#endif

    if (wtr && (!alarm_mode || c_alert_ping_left > 0))
    {
        alarm_mode = true;
        c_alert_ping_left--;
        _send(AQSH_MSG_REQ, 0,1,0);
    }
    else if (!wtr && alarm_mode)
    {
        alarm_mode = false;
        c_alert_ping_left = c_alert_ping;
        _send(AQSH_MSG_RES, 0,1,0);
    }
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_alarm_lgc] %lumcs: Sent message\n", micros());
#endif

    if (alarm_mode && loud_alarm)
    {
        digitalWrite(p_led, HIGH);
        tone(p_bzr, freq, 190);
        delay(100);
    }
#ifdef DEBUG_SENSOR        
    Serial.printf("[-_alarm_lgc] %lumcs: Done\n", micros());
#endif
}

void _slp(uint16_t _time)
{
    esp_sleep_enable_timer_wakeup(_time * 1000);

    esp_deep_sleep_start();
}

bool _chck_wtr()
{
#ifdef DEBUG_SENSOR        
    Serial.printf("[--_chck_wtr] %lumcs: Entering\n", micros());
    analogRead(p_wtr);
    Serial.printf("[--_chck_wtr] %lumcs: Done\n", micros());    
#endif
    return analogRead(p_wtr) > c_wtr_trshld;
}


bool _chck_btr()
{    
#ifdef DEBUG_SENSOR        
    Serial.printf("[--_chck_btr] %lumcs: Entering\n", micros());
    analogRead(p_btr);
    Serial.printf("[--_chck_btr] %lumcs: Done\n", micros());    
#endif
    return (analogRead(p_btr) / (float)ADC_MAX) * REF_VOLTAGE;
}


void _send(bool _type, bool _init, bool _wtr, bool _btr)
{
#ifdef DEBUG_SENSOR        
    Serial.printf("[--_send] %lumcs: Entering\n", micros());
#endif

    smsg.stm.id    = c_id;
    smsg.stm.type  = _type;
    smsg.stm.init  = _init;
    smsg.stm.wtr   = _wtr;
    smsg.stm.btr   = _btr;
    smsg.stm.crc   = _calc_crc(smsg.val);

    digitalWrite(p_tx_en, HIGH);
    o_tx.setRepeatTransmit(10);
    o_tx.enableTransmit(p_tx);
#ifdef DEBUG_SENSOR        
    Serial.printf("[--_send] %lumcs: Sending\n", micros());
#endif 
    o_tx.send(smsg.val, 32);
#ifdef DEBUG_SENSOR        
    Serial.printf("[--_send] %lumcs: Sent\n", micros());
#endif
    o_tx.disableTransmit();
    digitalWrite(p_tx_en, LOW);

#ifdef DEBUG_SENSOR        
    Serial.printf("[--_send] %lumcs: Done\n", micros());
#endif
}

uint8_t _calc_crc(uint32_t _val)
{
#ifdef DEBUG_SENSOR        
    Serial.printf("[--_calc_crc] %lumcs: Entering\n", micros());
#endif
    uint8_t crc = 0xff;
    uint8_t dat[4];

    dat[0] = (_val >> 24) & 0xff;
    dat[1] = (_val >> 16) & 0xff;
    dat[2] = (_val >> 8) & 0xff;
    dat[3] = _val & 0xff;

    for (int8_t i = 0; i < 4; i++)
    {
        crc ^= dat[i];
        for (int8_t j = 0; j < 8; j++)
        {
        if (crc & 0x80) 
        {
            crc = (crc << 1) ^ 0x31;
        }
        else 
        {
            crc <<= 1;
        }
        }
    }

#ifdef DEBUG_SENSOR        
    Serial.printf("[--_calc_crc] %lumcs: Done\n", micros());
#endif    
    return crc;
}

void loop()
{

}
