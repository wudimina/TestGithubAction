#include <jansson.h>
//#include "fcgio.h"
#include <fcgiapp.h>
#include <stdlib.h>
#include <string.h>
#include <rpcd/uci.h>
#include <ctype.h>
#include "xt_global.h"
#include "xt_db_ipc.h"
#include "xt_ipc.h"
#include "xt_ac_control_ipc.h"
#include "xt_schedule_ipc.h"
#include "xt_device_info.h"
#include "xt_mqtt_ipc.h"
#include "xt_wifimon_ipc.h"
#include "xt_update.h"
#include "xt_irext_ipc.h"
#include "xt_zigbee_ipc.h"
#include "xt_irin_ipc.h"
#include "xt_sensor_ipc.h"
#include "xt_license.h"
#include "xt_mtd.h"
#include "xt_hmi_ipc.h"
#include "xt_irout_ipc.h"

#include <fcntl.h>    /* For O_RDWR */
#include <unistd.h>   /* For open(), creat() */


/*!
 * \brief   Converts a char* ip address to a uint32_t value.
 * \param[in] ip  A pointer to a char array containing the ip string.
 * \return   returns a uint32_t version of the ip
 *           Note:  if the ip address has values greater than 255, they will
 *           be truncated.  If there are less than 4 values it will return 0.
 */
uint32_t ip_to_uint(const char* ip) {
    int a, b, c, d;
    uint32_t addr = 0;

    if (sscanf(ip, "%d.%d.%d.%d", &a, &b, &c, &d) != 4)
        return 0;

    addr = (a & 0xFF) << 24;
    addr |= (b & 0xFF) << 16;
    addr |= (c & 0xFF) << 8;
    addr |= (d & 0xFF);
    return addr;
}

/*!
 * \brief   Converts a uint32_t value to a char* containing the ip address.
 * \param[inout] ip_str  A pointer to a char array containing the ip string.
 * \param[in] ip  A uint32_t value representing an ip address
 * \return   returns the length of the returned string.
 */
int ip_to_string(char* ip_str, uint32_t ip) {
    sprintf(ip_str, "%d.%d.%d.%d",
            ( (ip & 0xFF000000) >> 24),
            ( (ip & 0x00FF0000) >> 16),
            ( (ip & 0x0000FF00) >> 8),
            (ip & 0x000000FF)
            );
    return strlen(ip_str);
}

void urldecode2(char *dst, const char *src)
{
        char a, b;
        while (*src) {
                if ((*src == '%') &&
                    ((a = src[1]) && (b = src[2])) &&
                    (isxdigit(a) && isxdigit(b))) {
                        if (a >= 'a')
                                a -= 'a'-'A';
                        if (a >= 'A')
                                a -= ('A' - 10);
                        else
                                a -= '0';
                        if (b >= 'a')
                                b -= 'a'-'A';
                        if (b >= 'A')
                                b -= ('A' - 10);
                        else
                                b -= '0';
                        *dst++ = 16*a+b;
                        src+=3;
                } else if (*src == '+') {
                        *dst++ = ' ';
                        src++;
                } else {
                        *dst++ = *src++;
                }
        }
        *dst++ = '\0';
}

#define XT_FCGI_MAX_CONTENT_LEN 4096
#define MAX_RECORD_GET 500

#define MAX_DEVICE  (10)

#define DEVICE_NAME_LENGTH (30)
typedef struct
{
    uint8_t deviceID;
    bool isIn;
    bool isThermostat;
    char deviceName[DEVICE_NAME_LENGTH];
}DeviceInfo_t;

typedef struct
{
    DeviceInfo_t device[MAX_DEVICE];
}DeviceList_t;

DeviceList_t HardcodedDevice =
{
    {
        {1, 1, 1 , "Main T1"}
        //{2, 1, 1},
        //{3, 0 ,0}
    }
};

int xt_uci_set(wifi_settings_t wifi);

#define XT_FCGI_SESSION_KEY_LEN 16
#define XT_FCGI_ADMIN_USERNAME  "admin"
#define XT_PASSWORD_DIGEST_LENGTH 32

typedef enum {
    XT_FCGI_STATUS_OK = 0,
    XT_FCGI_STATUS_BAD_REQUEST = 1,
    XT_FCGI_STATUS_UNAUTHORIZED = 2,
    XT_FCGI_STATUS_NOT_FOUND = 3,
    XT_FCGI_STATUS_METHOD_NOT_ALLOWED = 4,
    XT_FCGI_REQUEST_ENTITY_TOO_LARGE = 5,
    XT_FCGI_STATUS_LENGTH_REQUIRED = 6,
    XT_FCGI_STATUS_INTERNAL_ERROR = 7,
    XT_FCGI_STATUS_UNLICENSED_DEVICE = 8
} xt_fcgi_status;

static char g_xt_fcgi_session[XT_FCGI_SESSION_KEY_LEN * 2 + 1];
static char testpassword[30];

int testIR()
{
  uint16_t sample_irdata[]= {3400, 3500, 3600, 3700, 3800, 3900, 4000, 4100, 4200, 1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800, 1900, 2000, 2100};
  R_CONTROLCODE_LOG_t tempcontrolcode;
  uint8_t count;
  bool invalid=0;

  count = 21;
  xt_sendraw_ipc(sample_irdata, count);
  memset(&tempcontrolcode, 0x00, sizeof(R_CONTROLCODE_LOG_t));
  usleep(300000);
  xt_getlastir_log(&tempcontrolcode);

  if(count != tempcontrolcode.code.count)
  {
      invalid = 1;
  }    

  count = 19;
  xt_sendraw_ipc(sample_irdata, count);
  memset(&tempcontrolcode, 0x00, sizeof(R_CONTROLCODE_LOG_t));
  usleep(300000);
  xt_getlastir_log(&tempcontrolcode);
  if(count != tempcontrolcode.code.count)
  {
      invalid = 1;
  }

  count = 17;
  xt_sendraw_ipc(sample_irdata, count);
  memset(&tempcontrolcode, 0x00, sizeof(R_CONTROLCODE_LOG_t));
  usleep(300000);
  xt_getlastir_log(&tempcontrolcode);
  if(count != tempcontrolcode.code.count)
  {
      invalid = 1;
  }

  if(invalid)
  {
    return 0;
  }
  else
  {
    return 1;
  }
}

int8_t GetDeviceTable(DeviceList_t* pDeviceList)
{
    Devices_t device[MAX_DEVICE];
    uint8_t size;
    size = xt_getdevice_ipc(device, MAX_DEVICE);
    for(int i=0; i<size; i++)
    {
        HardcodedDevice.device[i].deviceID = device[i].deviceId;
        if(strcmp(device[i].type, "MAIN T1")==0)
        {
            HardcodedDevice.device[i].isIn = 1;
            HardcodedDevice.device[i].isThermostat = 1;
        }
        else if(strcmp(device[i].type, "Outdoor Sensor")==0)
        {
            HardcodedDevice.device[i].isIn = 0;
            HardcodedDevice.device[i].isThermostat = 0;
        }
        memcpy(HardcodedDevice.device[i].deviceName, device[i].label, DEVICE_NAME_LENGTH);
    }

    for(int i=size; i<MAX_DEVICE; i++)
    {
        memset(&HardcodedDevice.device[i], 0x00, sizeof(DeviceInfo_t));
    }

    memcpy(pDeviceList, &HardcodedDevice, sizeof(DeviceList_t));
    return 0;
}

bool getdirectorylength(const char* start, unsigned char* len)
{
    char * pch;
    bool ret = 1;
    pch = strchr((char*)(start + ((*start == '/')?1:0)),'/');
    if(pch == NULL)
    {
        pch = strchr((char*)(start + ((*start == '/')?1:0)),0);
        if(pch == start + ((*start == '/')?1:0))
        {
            ret = 0;
        }
    }

    *len = pch - start;
    return ret;
}

unsigned char asciitohex(const char* str, unsigned char len)
{
    unsigned char temp=0;
    for(int i=0; i<len-1; i++)
    {
        temp*=10;
        temp += ((*str) -'0');
        str++;
    }
    return temp;
}

/*!
 * \brief Get the content (body) from fcgi
 * \param[in] request The FCGX request
 * \param[out] buf The buffer that will hold the body, this has to be manually
 *                 freed
 */
static xt_fcgi_status xt_fcgi_get_content(FCGX_Request *request, char **buf)
{
    xt_fcgi_status status = XT_FCGI_STATUS_BAD_REQUEST;
    char *param_cl = FCGX_GetParam("CONTENT_LENGTH", request->envp);
    if (param_cl) {
        int content_length = atoi(param_cl);
        if (content_length < XT_FCGI_MAX_CONTENT_LEN) {
            *buf = (char *) malloc(content_length + 1);
            if (FCGX_GetStr(*buf, content_length, request->in) == content_length) {
                (*buf)[content_length] = 0;
                //status = XT_FCGI_STATUS_OK;
            }
        }
        else {
            //status = XT_FCGI_REQUEST_ENTITY_TOO_LARGE;
        }
    }
    else {
        //status = XT_FCGI_STATUS_LENGTH_REQUIRED;
    }

    //return status;
    return XT_FCGI_STATUS_OK;
}


/*!
 * \brief Get the content (body) from fcgi
 * \param[in] request The FCGX request
 * \param[out] buf The buffer that will hold the body, this has to be manually
 *                 freed
 */
static xt_fcgi_status xt_fcgi_get_content_len(FCGX_Request *request, char **buf, uint16_t* len)
{
    xt_fcgi_status status = XT_FCGI_STATUS_BAD_REQUEST;
    char *param_cl = FCGX_GetParam("CONTENT_LENGTH", request->envp);
    if (param_cl) {
        int content_length = atoi(param_cl);
        if (content_length < XT_FCGI_MAX_CONTENT_LEN) {
            *buf = (char *) malloc(content_length + 1);
            if (FCGX_GetStr(*buf, content_length, request->in) == content_length) {
                (*buf)[content_length] = 0;
                *len = content_length;
                //status = XT_FCGI_STATUS_OK;
            }
        }
        else {
            printf("too big: %d\n", content_length);
            *len=0;
            //status = XT_FCGI_REQUEST_ENTITY_TOO_LARGE;
        }
    }
    else {
        //status = XT_FCGI_STATUS_LENGTH_REQUIRED;
    }

    //return status;
    return XT_FCGI_STATUS_OK;
}

#define SET_WIFI_FORMAT "setwifi \"%s\" %s"
/*!
 * \brief Hash give password with SHA-256
 * \param[out] digest The output
 * \param[in] password The password to hash
 * \param[in] len The length of the password
 * \return True if successful, false otherwise
 */
bool xt_uc_password_digest(uint8_t *digest, const char *password, uint32_t len)
{
    /*
    int status = false;

    struct sockaddr_alg sa = {
        .salg_family = AF_ALG,
        .salg_type = "hash",
        .salg_name = "sha256"
    };

    int sock = socket(AF_ALG, SOCK_SEQPACKET, 0);
    if (sock > 0) {
        bind(sock, (struct sockaddr *)&sa, sizeof(sa));
        int opfd = accept(sock, NULL, 0);
        if (opfd > 0) {
            if (send(opfd, password, len, 0) == len &&
                    recv(opfd, digest, XT_PASSWORD_DIGEST_LENGTH, 0) == XT_PASSWORD_DIGEST_LENGTH) {
                status = true;
            }

            close(opfd);
        }

        close(sock);
    }

    return status;
    */
}


/*!
 * \brief Verify password stored in configuration file
 * \param[in] password
 * \param[in] len Length of the password
 * \return -1 on internal error, 0 if password is invalid, 1 if password is valid
 */
int xt_uc_password_verify(const char *password, uint32_t len)
{

    int status = -1;

    uint8_t digest[XT_PASSWORD_DIGEST_LENGTH];
    if (xt_uc_password_digest(digest, password, len)) {
        status = memcmp(password, testpassword, len) == 0;
    }

    return status;
}

/*!
 * \brief Verify password stored in configuration file
 * \param[in] old The previous password
 * \param[in] old_len Length of the previous password
 * \param[in] new The new password to set
 * \param[in] new_len Length of the new password
 * \return -1 on internal error, 0 if password is invalid, 1 if password is valid, -2 if the new
 *         password is empty
 */
int xt_uc_password_set(const char *old, uint32_t old_len, const char *new_pass, uint32_t new_len)
{
    int status = -1;

    if (new_len) {
        status = xt_uc_password_verify(old, old_len);
        if (status == 1) {
            uint8_t digest[XT_PASSWORD_DIGEST_LENGTH];
            //if (xt_uc_password_digest(digest, new, new_len)) {
            //    memcpy(g_xt_device_conf->password, digest, XT_PASSWORD_DIGEST_LENGTH);
            //    XT_CONFIG_MOD_MEMBER_ARR(g_xt_device_conf->password);
            //    status = 1;
            //}
            memcpy(testpassword, new_pass, new_len);
            status =1;
        }
    }
    else {
        // Cannot set empty password
        status = -2;
    }

    return status;
    

    return 1;
}

#define XT_MASTER_KEY_LENGTH 16

/*!
 * \brief Reset the password using the master key
 * \param[in] key
 *
 * \return
 */
int xt_uc_password_reset(const char *key)
{
    /*
    int status = -1;

    struct tm *tblock = NULL;
    struct timeval tv;

    if (gettimeofday(&tv, NULL) == 0) {
        char t_buff[XT_PASSWORD_DIGEST_LENGTH];
        tblock = (struct tm *)localtime((const time_t *)&tv.tv_sec);

        strftime(t_buff, XT_PASSWORD_DIGEST_LENGTH, "%d-%m-%Y", tblock);

        char msg[255];
        sprintf(msg, "adf-mont-kiara-%s", t_buff);

        uint8_t digest[XT_PASSWORD_DIGEST_LENGTH];
        if (xt_uc_password_digest(digest, msg, strlen(msg))) {
            // 32 characters only, 16 bytes
            char *t_digest = t_buff;
            for (int i = 0; i < XT_MASTER_KEY_LENGTH; i++) {
                t_digest += sprintf(t_digest, "%02X", digest[i]);
            }

            status = memcmp(key, t_buff, XT_MASTER_KEY_LENGTH * 2) == 0;
            if (status) {
                // Reset to default
                memcpy(g_xt_device_conf->password, XT_DEFAULT_PASSWORD, XT_PASSWORD_DIGEST_LENGTH);
                XT_CONFIG_MOD_MEMBER_ARR(g_xt_device_conf->password);
            }
        }
    }

    return status;
    */

    return 1;
}

/*!
 * \brief Handler for auth resource POST request
 * \param[out] root
 * \param[in] request
 *
 * return XT_FCGI_STATUS_OK if successful.
 */
static xt_fcgi_status xt_fcgi_auth_post(json_t *root, FCGX_Request *request)
{
    xt_fcgi_status status = XT_FCGI_STATUS_OK;
    char *buf = NULL;

    status = xt_fcgi_get_content(request, &buf);
    if (status == XT_FCGI_STATUS_OK) {
        json_error_t jerror;

        json_t *content = json_loads(buf, 0, &jerror);
        if (content) {
            if (json_is_object(content)) {
                json_t *username_val = json_object_get(content, "username");
                json_t *password_val = json_object_get(content, "password");
                if (password_val && json_is_string(password_val)
                && username_val && json_is_string(username_val)) {
                    const char *username = json_string_value(username_val);
                    const char *password = json_string_value(password_val);
                    uint32_t password_len = strlen(password);

                    if (strcmp(username, XT_FCGI_ADMIN_USERNAME) == 0) {
                        int res = xt_uc_password_verify(password, password_len);
                        if (res == 1) {
                            // Create session key
                            uint8_t rand_data[XT_FCGI_SESSION_KEY_LEN];
                            int fd = open("/dev/urandom", O_RDONLY);
                            if (fd > 0) {
                                ssize_t result = read(fd, rand_data, XT_FCGI_SESSION_KEY_LEN);
                                if (result > 0)
                                {
                                    char *t_session = g_xt_fcgi_session;
                                    for (int i = 0; i < 16; i++) {
                                        t_session += sprintf(t_session, "%02X", rand_data[i]);
                                    }

                                    char h_buffer[0xFF];
                                    sprintf(h_buffer, "Set-cookie: auth=%s\r\n", g_xt_fcgi_session);
                                    FCGX_PutStr(h_buffer, strlen(h_buffer), request->out);

                                    // Also return in JSON
                                    json_object_set_new(root, "auth", json_string(g_xt_fcgi_session));
                                }
                                close(fd);
                            }
                        }
                        else if (res == 0) {
                            status = XT_FCGI_STATUS_BAD_REQUEST;
                        }
                        else {
                            status = XT_FCGI_STATUS_INTERNAL_ERROR;
                        }
                    }
                    else {
                        status = XT_FCGI_STATUS_BAD_REQUEST;
                    }
                }
                else {
                    status = XT_FCGI_STATUS_BAD_REQUEST;
                }
            }
            else {
                status = XT_FCGI_STATUS_BAD_REQUEST;
            }

            json_decref(content);
        }
        else {
            status = XT_FCGI_STATUS_BAD_REQUEST;
            json_object_set_new(root, "message", json_string(jerror.text));
        }

        free(buf);
    }

    return status;
}

/*!
 * \brief Handler for auth resource PATCH request
 * \param[out] root
 * \param[in] request
 *
 * return XT_FCGI_STATUS_OK if successful.
 */
static xt_fcgi_status xt_fcgi_auth_patch(json_t *root, FCGX_Request *request)
{
    xt_fcgi_status status = XT_FCGI_STATUS_OK;
    char *buf = NULL;

    status = xt_fcgi_get_content(request, &buf);
    if (status == XT_FCGI_STATUS_OK) {
        json_error_t jerror;

        json_t *content = json_loads(buf, 0, &jerror);
        if (content) {
            if (json_is_object(content)) {
                json_t *n_password_val = json_object_get(content, "new_password");
                json_t *o_password_val = json_object_get(content, "old_password");
                if (o_password_val && json_is_string(o_password_val)
                && n_password_val && json_is_string(n_password_val)) {
                    const char *o_password = json_string_value(o_password_val);
                    const char *n_password = json_string_value(n_password_val);

                    int res = xt_uc_password_set(o_password, strlen(o_password),
                            n_password, strlen(n_password));
                    if (res == 1) {
                        // Nothing else to do
                    }
                    else if (res == 0 || res == -2) {
                        status = XT_FCGI_STATUS_BAD_REQUEST;
                    }
                    else {
                        status = XT_FCGI_STATUS_INTERNAL_ERROR;
                    }
                }
            }
            else {
                status = XT_FCGI_STATUS_BAD_REQUEST;
            }

            json_decref(content);
        }
        else {
            status = XT_FCGI_STATUS_BAD_REQUEST;
            json_object_set_new(root, "message", json_string(jerror.text));
        }

        free(buf);
    }

    return status;
}

/*!
 * \brief Handler for master_auth resource PATCH request
 * \param[out] root
 * \param[in] request
 *
 * return XT_FCGI_STATUS_OK if successful.
 */
static xt_fcgi_status xt_fcgi_master_auth_patch(json_t *root, FCGX_Request *request)
{
    xt_fcgi_status status = XT_FCGI_STATUS_OK;
    char *buf = NULL;

    status = xt_fcgi_get_content(request, &buf);
    if (status == XT_FCGI_STATUS_OK) {
        json_error_t jerror;

        json_t *content = json_loads(buf, 0, &jerror);
        if (content) {
            if (json_is_object(content)) {
                json_t *master_val = json_object_get(content, "key");
                if (master_val && json_is_string(master_val)) {
                    const char *master = json_string_value(master_val);
                    int res = xt_uc_password_reset(master);
                    if (res == 0) {
                        // Wrong key
                        status = XT_FCGI_STATUS_BAD_REQUEST;
                    }
                    else if (res == -1) {
                        status = XT_FCGI_STATUS_INTERNAL_ERROR;
                    }
                }
                else {
                    status = XT_FCGI_STATUS_BAD_REQUEST;
                }
            }
            else {
                status = XT_FCGI_STATUS_BAD_REQUEST;
            }

            json_decref(content);
        }
        else {
            status = XT_FCGI_STATUS_BAD_REQUEST;
            json_object_set_new(root, "message", json_string(jerror.text));
        }

        free(buf);
    }

    return status;
}

static xt_fcgi_status xt_fcgi_auth_delete(json_t *root, FCGX_Request *request)
{
    memset(g_xt_fcgi_session, 0, sizeof(g_xt_fcgi_session));
    return XT_FCGI_STATUS_OK;
}

static bool xt_fcgi_auth_verify(json_t *root, FCGX_Request *request)
{
    bool status = false;
    char *cookie = FCGX_GetParam("HTTP_COOKIE", request->envp);
    if (cookie) {
        char *head = strstr(cookie, "auth");
        if (head) {
            head += 4;
            while(*head && (*head == ' ' || *head == '=')) {
                head++;
            }

            char *tail = head;
            while(*tail && *tail != ' ' && *tail != ';') {
                tail++;
            }

            if (tail - head == (XT_FCGI_SESSION_KEY_LEN * 2)) {
                status = strncmp(head, g_xt_fcgi_session, XT_FCGI_SESSION_KEY_LEN * 2) == 0;
            }
        }
    }

    return status;
}

#define SET_STATIC_FORMAT "setipmode %s %s %s %s %s"
#define SET_DHCP_FORMAT "setipmode dhcp"
#define SET_HIDDEN_WIFI_FORMAT "sethiddenwifi \"%s\" %s %s %s %d"

int main(void) {
    // Backup the stdio streambufs
    //streambuf * cin_streambuf  = cin.rdbuf();
    //streambuf * cout_streambuf = cout.rdbuf();
    //streambuf * cerr_streambuf = cerr.rdbuf();
    uint16_t i = 0;
    FCGX_Request request;
    char macstr[30];
    //InitACSetting();
    setvbuf(stdout, NULL, _IOLBF, 0);
    memcpy(testpassword, "123adftech123", sizeof("123adftech123"));

    printf("init fcgi\n");

    FCGX_Init();
    FCGX_InitRequest(&request, 0, 0);

    //printf("sensorlog size: %d, %d\r\n", sizeof(DeviceSensorData_t),sizeof(DeviceSensorData_t)*MAX_RECORD_GET);
    //printf("aclog size: %d, %d\r\n", sizeof(ACLog_t),sizeof(ACLog_t)*MAX_RECORD_GET);
    //printf("alarmlog size: %d, %d\r\n", sizeof(AlarmLog_t),sizeof(AlarmLog_t)*MAX_RECORD_GET);

    wifi_settings_t wifi;


    while (FCGX_Accept_r(&request) == 0) {
        char reply_json =1;
        json_t *root = json_object();

        const char *param_method = FCGX_GetParam("REQUEST_METHOD", request.envp);
        const char *param_ru = FCGX_GetParam("REQUEST_URI", request.envp);

        char *output = (char*)malloc(strlen(param_ru)+1);
        urldecode2(output, param_ru);
        param_ru = output;
        //for /fcgi/, force the param_ru to forward 5 bytes
        param_ru += 5;

        if(xt_fcgi_auth_verify(NULL, &request))
        {
            printf("auth success\r\n");
        }
        else
        {
            printf("auth failed\r\n");
        }
        
        if (strcmp(param_method, "POST") == 0 && strcmp(param_ru, "/auth") == 0) {
            // Creation
            xt_fcgi_auth_post(root, &request);
        }
        else if(strcmp(param_method, "PATCH") == 0 && strcmp(param_ru, "/auth") == 0)
        {
            xt_fcgi_auth_patch(root, &request);
        }
        else if(strcmp(param_method, "DELETE") == 0 && strcmp(param_ru, "/auth") == 0)
        {
            xt_fcgi_auth_delete(root, &request);
        }
        else if (strcmp(param_method, "GET") == 0) {
            if(strcmp(param_ru, "/platform") == 0)
            {
                json_object_set_new(root, "platform", json_string("local"));
            }
            else if (strcmp(param_ru, "/devices") == 0)
            {
                printf("got devices\n");
                DeviceList_t deviceList;
                GetDeviceTable(&deviceList);

                json_t *dList = json_array();
                //json_object_set_new(root, "Home Page", json_string("Hello World"));

                for(i=0; i<MAX_DEVICE ; i++)
                {
                    json_t *device = json_object();
                    if(deviceList.device[i].deviceID != 0)
                    {
                        json_object_set_new(device, "index", json_integer(deviceList.device[i].deviceID));
                        json_object_set_new(device, "isIn", json_boolean(deviceList.device[i].isIn));
                        json_object_set_new(device, "isThermostat", json_boolean(deviceList.device[i].isThermostat));
                        json_object_set_new(device, "label", json_string(deviceList.device[i].deviceName));
                        if(xt_device_mac(macstr) <= 0)
                        {
                            memcpy(macstr, "NOT FOUND", sizeof("NOT FOUND"));
                        }
                        json_object_set_new(device, "mac", json_string((const char *)macstr));
                        json_array_append_new(dList, device);
                    }
                }
                json_object_set_new(root, "Device List", dList);
            }
            else if(strcmp(param_ru, "/deviceID") == 0)
            {
                if(xt_device_mac(macstr) <= 0)
                {
                    memcpy(macstr, "NOT FOUND", sizeof("NOT FOUND"));
                }

                json_object_set_new(root, "deviceId", json_string((const char *)macstr));
            }
            else if(strcmp(param_ru, "/deviceloc") == 0)
            {
                T_LOCATION deviceloc;
                if(xt_device_location(&deviceloc) != 0)
                {
                    memset(&deviceloc, 0x00, sizeof(T_LOCATION));
                    json_object_set_new(root, "status", json_string("NG"));
                }
                json_object_set_new(root, "status", json_string("OK"));
                json_object_set_new(root, "lat", json_real(deviceloc.lat));
                json_object_set_new(root, "lon", json_real(deviceloc.lon));
            }
            else if (strncmp(param_ru, "/version", 8) == 0)
            {
                char hwversion[20];
                char swversion[20];
                int hwres, swres;
                hwres = xt_get_boardtype(hwversion);
                swres = xt_get_t1_version(swversion);
                json_object_set_new(root, "hwversion", json_string(hwres ? "unknown" : (const char *)hwversion));
                json_object_set_new(root, "swversion", json_string(swres ? "unknown" : (const char *)swversion));
                if(!swres && !hwres)
                {
                    json_object_set_new(root, "status", json_string("OK"));
                }
                else
                {
                    json_object_set_new(root, "status", json_string("NG"));
                }
            }
            else if (strncmp(param_ru, "/hwversion", 10) == 0)
            {
                char version[20];
                if(xt_get_boardtype(version) == 0) {
                    json_object_set_new(root, "status", json_string("OK"));
                    json_object_set_new(root, "hwversion", json_string((const char *)version));
                }
                else {
                    json_object_set_new(root, "status", json_string("NG"));
                }
            }
            else if (strncmp(param_ru, "/swversion", 10) == 0)
            {
                char version[20];
                if(xt_get_t1_version(version) == 0) {
                    json_object_set_new(root, "status", json_string("OK"));
                    json_object_set_new(root, "swversion", json_string((const char *)version));
                }
                else {
                    json_object_set_new(root, "status", json_string("NG"));
                }
            }
            else if (strncmp(param_ru, "/sensordata", 11) == 0)
            {
                unsigned char len;
                DateTime_t datetime;
                DeviceSensorData_t sensorData;
                uint8_t deviceID = 0;

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }
                //param_ru += 11;

                if (strncmp(param_ru, "/1", 2) == 0) {
                    deviceID = 1;
                }
                else if (strncmp(param_ru, "/2", 2) == 0) {
                    deviceID = 2;
                }
                else if (strncmp(param_ru, "/3", 2) == 0) {
                    deviceID = 3;
                }
                else
                {
                    deviceID = 1;
                }

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }

                //clear all date time 
                memset(&datetime, 0x00, sizeof(DateTime_t));
                
                if(strncmp(param_ru, "/latest", 7) == 0)
                {
                    xt_currentsensorlog_ipc(deviceID, &sensorData);
                }
                else
                {
                    if(getdirectorylength(param_ru,&len))
                    {
                        //date retrieval
                        //TODO:validate date
                        if(len == DATE_LENGTH)
                        {
                            //perform cpy
                            memcpy(datetime.date, param_ru+1, DATE_LENGTH - 1);
                            datetime.date[DATE_LENGTH-1] = 0;
                        }
                        param_ru += len;
                    }


                    if(getdirectorylength(param_ru,&len))
                    {
                        //date retrieval
                        //TODO:validate time
                        if(len == TIME_LENGTH)
                        {
                            //perform cpy
                            memcpy(datetime.time, param_ru+1, TIME_LENGTH - 1);
                            datetime.time[TIME_LENGTH-1] = 0;
                        }
                        param_ru += len;
                    }

                    //memcpy(datetime.time, "09:00:00", 9);
                    //memcpy(datetime.date, "2018-06-05", 11);

                    //GetSensorData(deviceID, &sensorData, &datetime);
                    //xt_getsensor_ipc(deviceID, &datetime, &sensorData);
                }
                
                json_object_set_new(root, "Date", json_string((const char *)sensorData.dateTime.date));
                json_object_set_new(root, "Time", json_string((const char *)sensorData.dateTime.time));
                json_object_set_new(root, "temperature", json_real(sensorData.temperature));
                json_object_set_new(root, "humidity", json_real(sensorData.humidity));
                
            }
            else if (strncmp(param_ru, "/AC", 3) == 0)
            {
                uint8_t deviceID = 1;
                ACSetting_t acSetting;
                unsigned char len;
                bool ACStatus;

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }
                //param_ru += 11;

                if (strncmp(param_ru, "/1", 2) == 0) {
                    deviceID = 1;
                }
                else if (strncmp(param_ru, "/2", 2) == 0) {
                    deviceID = 2;
                }
                else if (strncmp(param_ru, "/3", 2) == 0) {
                    deviceID = 3;
                }

                //GetAC(deviceID, &acSetting, &ACStatus);
                xt_getac_ipc(deviceID, &acSetting, &ACStatus);
                json_object_set_new(root, "temp", json_integer(acSetting.temp));
                json_object_set_new(root, "fanspeed", json_integer(acSetting.fanspeed));
                json_object_set_new(root, "mode", json_integer(acSetting.mode));
                json_object_set_new(root, "isSwing", json_boolean(acSetting.isSwing));
                json_object_set_new(root, "ACStatus", json_boolean(ACStatus));
            }
            else if (strncmp(param_ru, "/acdata", 7) == 0)
            {
                unsigned char len;
                DateTime_t datetime;
                ACLog_t acLog;
                uint8_t deviceID = 0;

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }
                //param_ru += 11;

                if (strncmp(param_ru, "/1", 2) == 0) {
                    deviceID = 1;
                }
                else if (strncmp(param_ru, "/2", 2) == 0) {
                    deviceID = 2;
                }
                else if (strncmp(param_ru, "/3", 2) == 0) {
                    deviceID = 3;
                }
                else
                {
                    deviceID = 1;
                }

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }

                //clear all date time 
                memset(&datetime, 0x00, sizeof(DateTime_t));
                if(getdirectorylength(param_ru,&len))
                {
                    //date retrieval
                    //TODO:validate date
                    if(len == DATE_LENGTH)
                    {
                        //perform cpy
                        memcpy(datetime.date, param_ru+1, DATE_LENGTH - 1);
                        datetime.date[DATE_LENGTH-1] = 0;
                    }
                    param_ru += len;
                }


                if(getdirectorylength(param_ru,&len))
                {
                    //date retrieval
                    //TODO:validate time
                    if(len == TIME_LENGTH)
                    {
                        //perform cpy
                        memcpy(datetime.time, param_ru+1, TIME_LENGTH - 1);
                        datetime.time[TIME_LENGTH-1] = 0;
                    }
                    param_ru += len;
                }

                //memcpy(datetime.time, "09:00:00", 9);
                //memcpy(datetime.date, "2018-06-05", 11);

                xt_getacdata_ipc(deviceID, &datetime, &acLog);
                json_object_set_new(root, "Date", json_string((const char *)acLog.dateTime.date));
                json_object_set_new(root, "Time", json_string((const char *)acLog.dateTime.time));
                json_object_set_new(root, "temp", json_integer(acLog.ACSetting.temp));
                json_object_set_new(root, "fanspeed", json_integer(acLog.ACSetting.fanspeed));
                json_object_set_new(root, "mode", json_integer(acLog.ACSetting.mode));
                json_object_set_new(root, "isSwing", json_boolean(acLog.ACSetting.isSwing));
                json_object_set_new(root, "turnOnOFF", json_boolean(acLog.turnOnOFF));
                json_object_set_new(root, "ACStatus", json_boolean(acLog.acStatus));
                json_object_set_new(root, "eventSrc", json_integer(acLog.eventSrc));
                //json_object_set_new(root, "Date", json_string((const char *)sensorData.dateTime.date));
                //json_object_set_new(root, "Time", json_string((const char *)sensorData.dateTime.time));
                //json_object_set_new(root, "temperature", json_real(sensorData.temperature));
                //json_object_set_new(root, "humidity", json_real(sensorData.humidity));
            }
            else if(strncmp(param_ru, "/LogAC", 6) == 0)
            {
                uint8_t deviceID = 1;
                ACLog_t logtemp[MAX_RECORD_GET];
                DateTime_t start;
                DateTime_t end;
                uint16_t count;
                json_t *eList = json_array();
                uint8_t len;

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }
                //param_ru += 11;

                if (strncmp(param_ru, "/1", 2) == 0) {
                    deviceID = 1;
                }
                else if (strncmp(param_ru, "/2", 2) == 0) {
                    deviceID = 2;
                }
                else if (strncmp(param_ru, "/3", 2) == 0) {
                    deviceID = 3;
                }

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }

                //clear all date time 
                memset(&end, 0x00, sizeof(DateTime_t));
                memset(&start, 0x00, sizeof(DateTime_t));

                if(getdirectorylength(param_ru,&len))
                {
                    //date retrieval
                    //TODO:validate date
                    if(len == DATE_LENGTH)
                    {
                        //perform cpy
                        memcpy(start.date, param_ru+1, DATE_LENGTH - 1);
                        start.date[DATE_LENGTH-1] = 0;
                    }
                    param_ru += len;
                }


                if(getdirectorylength(param_ru,&len))
                {
                    //date retrieval
                    //TODO:validate time
                    if(len == TIME_LENGTH)
                    {
                        //perform cpy
                        memcpy(start.time, param_ru+1, TIME_LENGTH - 1);
                        start.time[TIME_LENGTH-1] = 0;
                    }
                    param_ru += len;
                }

                if(getdirectorylength(param_ru,&len))
                {
                    //date retrieval
                    //TODO:validate date
                    if(len == DATE_LENGTH)
                    {
                        //perform cpy
                        memcpy(end.date, param_ru+1, DATE_LENGTH - 1);
                        end.date[DATE_LENGTH-1] = 0;
                    }
                    param_ru += len;
                }


                if(getdirectorylength(param_ru,&len))
                {
                    //date retrieval
                    //TODO:validate time
                    if(len == TIME_LENGTH)
                    {
                        //perform cpy
                        memcpy(end.time, param_ru+1, TIME_LENGTH - 1);
                        end.time[TIME_LENGTH-1] = 0;
                    }
                    param_ru += len;
                }
                // memcpy(start.date, "2018-10-10", DATE_LENGTH);
                // memcpy(start.time, "21:00:00", TIME_LENGTH);
                // memcpy(end.date, "2018-10-12", DATE_LENGTH);
                // memcpy(end.time, "03:00:00", TIME_LENGTH);

                count = xt_aclog_ipc(logtemp, deviceID, &start, &end, MAX_RECORD_GET);
                //put into json
                for(int i=0; i<count; i++)
                {
                    json_t *device = json_object();

                    json_object_set_new(device, "Date", json_string((const char *)logtemp[i].dateTime.date));
                    json_object_set_new(device, "Time", json_string((const char *)logtemp[i].dateTime.time));
                    json_object_set_new(device, "temp", json_integer(logtemp[i].ACSetting.temp));
                    json_object_set_new(device, "fanspeed", json_integer(logtemp[i].ACSetting.fanspeed));
                    json_object_set_new(device, "mode", json_integer(logtemp[i].ACSetting.mode));
                    json_object_set_new(device, "isSwing", json_boolean(logtemp[i].ACSetting.isSwing));
                    json_object_set_new(device, "turnOnOFF", json_boolean(logtemp[i].turnOnOFF));
                    json_object_set_new(device, "ACStatus", json_boolean(logtemp[i].acStatus));
                    json_object_set_new(device, "eventSrc", json_integer(logtemp[i].eventSrc));
                    json_array_append_new(eList, device);
                }

                json_object_set_new(root, "AC Log", eList);

            }
            else if(strncmp(param_ru, "/LogSensor", 10) == 0)
            {
                uint8_t deviceID = 1;
                DeviceSensorData_t logsensor[MAX_RECORD_GET];
                DateTime_t start;
                DateTime_t end;
                uint16_t count;
                json_t *eList = json_array();
                uint8_t len;

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }
                //param_ru += 11;

                if (strncmp(param_ru, "/1", 2) == 0) {
                    deviceID = 1;
                }
                else if (strncmp(param_ru, "/2", 2) == 0) {
                    deviceID = 2;
                }
                else if (strncmp(param_ru, "/3", 2) == 0) {
                    deviceID = 3;
                }

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }


                //clear all date time 
                memset(&end, 0x00, sizeof(DateTime_t));
                memset(&start, 0x00, sizeof(DateTime_t));

                if(getdirectorylength(param_ru,&len))
                {
                    //date retrieval
                    //TODO:validate date
                    if(len == DATE_LENGTH)
                    {
                        //perform cpy
                        memcpy(start.date, param_ru+1, DATE_LENGTH - 1);
                        start.date[DATE_LENGTH-1] = 0;
                    }
                    param_ru += len;
                }


                if(getdirectorylength(param_ru,&len))
                {
                    //date retrieval
                    //TODO:validate time
                    if(len == TIME_LENGTH)
                    {
                        //perform cpy
                        memcpy(start.time, param_ru+1, TIME_LENGTH - 1);
                        start.time[TIME_LENGTH-1] = 0;
                    }
                    param_ru += len;
                }

                if(getdirectorylength(param_ru,&len))
                {
                    //date retrieval
                    //TODO:validate date
                    if(len == DATE_LENGTH)
                    {
                        //perform cpy
                        memcpy(end.date, param_ru+1, DATE_LENGTH - 1);
                        end.date[DATE_LENGTH-1] = 0;
                    }
                    param_ru += len;
                }


                if(getdirectorylength(param_ru,&len))
                {
                    //date retrieval
                    //TODO:validate time
                    if(len == TIME_LENGTH)
                    {
                        //perform cpy
                        memcpy(end.time, param_ru+1, TIME_LENGTH - 1);
                        end.time[TIME_LENGTH-1] = 0;
                    }
                    param_ru += len;
                }
                // memcpy(start.date, "2018-10-10", DATE_LENGTH);
                // memcpy(start.time, "21:00:00", TIME_LENGTH);
                // memcpy(end.date, "2018-10-12", DATE_LENGTH);
                // memcpy(end.time, "03:00:00", TIME_LENGTH);
                count = xt_dbsensorlog_ipc(logsensor, deviceID, &start, &end, MAX_RECORD_GET);
                //put into json
                for(int i=0; i<count; i++)
                {
                    json_t *device = json_object();

                    json_object_set_new(device, "Date", json_string((const char *)logsensor[i].dateTime.date));
                    json_object_set_new(device, "Time", json_string((const char *)logsensor[i].dateTime.time));
                    json_object_set_new(device, "temperature", json_real(logsensor[i].temperature));
                    json_object_set_new(device, "humidity", json_real(logsensor[i].humidity));

                    json_array_append_new(eList, device);
                }

                json_object_set_new(root, "Sensor Log", eList);

            }
            else if(strncmp(param_ru, "/DayLogSensor", 10) == 0)
            {
                uint8_t deviceID = 1;
                DeviceSensorData_t logsensor[MAX_RECORD_GET];
                DateTime_t start;
                DateTime_t end;
                uint16_t count;
                json_t *eList = json_array();
                uint8_t len;

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }
                //param_ru += 11;

                if (strncmp(param_ru, "/1", 2) == 0) {
                    deviceID = 1;
                }
                else if (strncmp(param_ru, "/2", 2) == 0) {
                    deviceID = 2;
                }
                else if (strncmp(param_ru, "/3", 2) == 0) {
                    deviceID = 3;
                }

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }


                //clear all date time 
                memset(&end, 0x00, sizeof(DateTime_t));
                memset(&start, 0x00, sizeof(DateTime_t));

                if(getdirectorylength(param_ru,&len))
                {
                    //date retrieval
                    //TODO:validate date
                    if(len == DATE_LENGTH)
                    {
                        //perform cpy
                        memcpy(start.date, param_ru+1, DATE_LENGTH - 1);
                        start.date[DATE_LENGTH-1] = 0;
                    }
                    param_ru += len;
                }
                // memcpy(start.date, "2018-10-10", DATE_LENGTH);
                // memcpy(start.time, "21:00:00", TIME_LENGTH);
                // memcpy(end.date, "2018-10-12", DATE_LENGTH);
                // memcpy(end.time, "03:00:00", TIME_LENGTH);
                count = xt_dbsensordaylog_ipc(logsensor, deviceID, &start, MAX_RECORD_GET);
                //put into json
                for(int i=0; i<count; i++)
                {
                    json_t *device = json_object();

                    json_object_set_new(device, "Date", json_string((const char *)logsensor[i].dateTime.date));
                    json_object_set_new(device, "Time", json_string((const char *)logsensor[i].dateTime.time));
                    json_object_set_new(device, "temperature", json_real(logsensor[i].temperature));
                    json_object_set_new(device, "humidity", json_real(logsensor[i].humidity));

                    json_array_append_new(eList, device);
                }

                json_object_set_new(root, "Sensor Log", eList);

            }
            else if(strncmp(param_ru, "/LogAlarm", 9) == 0)
            {
                uint8_t deviceID = 1;
                AlarmLog_t logalarm[MAX_RECORD_GET];
                DateTime_t start;
                DateTime_t end;
                uint16_t count;
                json_t *eList = json_array();
                uint8_t len;

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }
                //param_ru += 11;

                if (strncmp(param_ru, "/1", 2) == 0) {
                    deviceID = 1;
                }
                else if (strncmp(param_ru, "/2", 2) == 0) {
                    deviceID = 2;
                }
                else if (strncmp(param_ru, "/3", 2) == 0) {
                    deviceID = 3;
                }

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }

                //clear all date time 
                memset(&end, 0x00, sizeof(DateTime_t));
                memset(&start, 0x00, sizeof(DateTime_t));
                
                if(getdirectorylength(param_ru,&len))
                {
                    //date retrieval
                    //TODO:validate date
                    if(len == DATE_LENGTH)
                    {
                        //perform cpy
                        memcpy(start.date, param_ru+1, DATE_LENGTH - 1);
                        start.date[DATE_LENGTH-1] = 0;
                    }
                    param_ru += len;
                }

                if(getdirectorylength(param_ru,&len))
                {
                    //date retrieval
                    //TODO:validate time
                    if(len == TIME_LENGTH)
                    {
                        //perform cpy
                        memcpy(start.time, param_ru+1, TIME_LENGTH - 1);
                        start.time[TIME_LENGTH-1] = 0;
                    }
                    param_ru += len;
                }

                if(getdirectorylength(param_ru,&len))
                {
                    //date retrieval
                    //TODO:validate date
                    if(len == DATE_LENGTH)
                    {
                        //perform cpy
                        memcpy(end.date, param_ru+1, DATE_LENGTH - 1);
                        end.date[DATE_LENGTH-1] = 0;
                    }
                    param_ru += len;
                }


                if(getdirectorylength(param_ru,&len))
                {
                    //date retrieval
                    //TODO:validate time
                    if(len == TIME_LENGTH)
                    {
                        //perform cpy
                        memcpy(end.time, param_ru+1, TIME_LENGTH - 1);
                        end.time[TIME_LENGTH-1] = 0;
                    }
                    param_ru += len;
                }
                // memcpy(start.date, "2018-10-10", DATE_LENGTH);
                // memcpy(start.time, "21:00:00", TIME_LENGTH);
                // memcpy(end.date, "2018-10-12", DATE_LENGTH);
                // memcpy(end.time, "03:00:00", TIME_LENGTH);
                memset(logalarm, 0x00, sizeof(logalarm));
                count = xt_alarmlog_ipc(logalarm, deviceID, &start, &end, MAX_RECORD_GET);
                //count =1;
                //put into json
                for(int i=0; i<count; i++)
                {
                    json_t *device = json_object();
                    json_object_set_new(device, "Date", json_string((const char *)logalarm[i].dateTime.date));
                    json_object_set_new(device, "Time", json_string((const char *)logalarm[i].dateTime.time));
                    if(logalarm[i].eventType[0])
                    {
                        json_object_set_new(device, "eventType", json_string((const char *)logalarm[i].eventType));
                    }

                    if(logalarm[i].extraInfo[0])
                    {
                        json_object_set_new(device, "extraInfo", json_string((const char *)logalarm[i].extraInfo));
                    }

                    if(logalarm[i].eventInfo[0])
                    {
                        json_object_set_new(device, "eventInfo", json_string((const char *)logalarm[i].eventInfo));
                    }
                    
                    json_array_append_new(eList, device);
                }

                json_object_set_new(root, "Alarm Log", eList);
            }
            else if(strncmp(param_ru, "/schedule", 9) == 0)
            {
                uint16_t count;
                json_t *eList = json_array();
                xt_schedule_t schedule_setting[NUMBER_OF_SCHEDULE];
                uint8_t timebuffer[TIME_LENGTH];
                memset(schedule_setting, 0x00, sizeof(schedule_setting));
                count = xt_getschedule_ipc(schedule_setting, NUMBER_OF_SCHEDULE);
                //count =1;
                //put into json
                for(int i=0; i<count; i++)
                {
                    json_t *device = json_object();
                    sprintf((char*)timebuffer, "%02d:%02d:00", schedule_setting[i].hours, schedule_setting[i].minutes);

                    json_object_set_new(device, "index", json_integer(i));
                    json_object_set_new(device, "Date", json_string((const char *)schedule_setting[i].specific_date));
                    json_object_set_new(device, "Time", json_string((const char *)timebuffer));
                    json_object_set_new(device, "deviceId", json_integer(schedule_setting[i].acSetting.deviceID));
                    json_object_set_new(device, "temp", json_integer(schedule_setting[i].acSetting.ACSetting.temp));
                    json_object_set_new(device, "temp", json_integer(schedule_setting[i].acSetting.ACSetting.temp));
                    json_object_set_new(device, "fanspeed", json_integer(schedule_setting[i].acSetting.ACSetting.fanspeed));
                    json_object_set_new(device, "mode", json_integer(schedule_setting[i].acSetting.ACSetting.mode));
                    json_object_set_new(device, "isSwing", json_boolean(schedule_setting[i].acSetting.ACSetting.isSwing));
                    json_object_set_new(device, "turnOnOFF", json_boolean(schedule_setting[i].acSetting.acStatus));
                    json_object_set_new(device, "days", json_integer(schedule_setting[i].days));
                    json_object_set_new(device, "enable", json_boolean(schedule_setting[i].enabled));
                    json_object_set_new(device, "notify", json_boolean(schedule_setting[i].notify));
                    json_object_set_new(device, "aienabled", json_boolean(schedule_setting[i].aienabled));
                    json_array_append_new(eList, device);
                }

                json_object_set_new(root, "Schedule setting", eList);
            }
            else if(strncmp(param_ru, "/wifiaplist", 11) == 0)
            {
                T_AP_DATA ap_list[MAX_AP_LIST];
                json_t *eList = json_array();
                int count=0;
                char currentssid[100];
                if(xt_get_curr_ssid(currentssid) <= 0)
                {
                    //IP not found
                    memcpy(currentssid, "NO SSID", sizeof("NO SSID"));
                }
                printf("got ssid %s \n", currentssid);

                //count = xt_get_wifimon_ap_list(&ap_list[0]);
                if (count <= 0) count = xt_get_ap_list(&ap_list[0]);
                if(count>0)
                {
                    for(int i=0; i<count; i++)
                    {
                        json_t *ap = json_object();
                        json_object_set_new(ap, "channel ID", json_integer(ap_list[i].channelID));
                        json_object_set_new(ap, "SSID", json_string(ap_list[i].ssid));
                        json_object_set_new(ap, "BSSID", json_string((const char *)ap_list[i].mac_addr));
                        json_object_set_new(ap, "encryption", json_string((const char *)ap_list[i].encryption));
                        json_object_set_new(ap, "signal strength", json_integer(ap_list[i].signalStrength));
                        if(strcmp(ap_list[i].ssid, currentssid) == 0)
                        {
                            json_object_set_new(ap, "connected", json_boolean(1));
                        }
                        else
                        {
                            json_object_set_new(ap, "connected", json_boolean(0));
                        }
                        json_array_append_new(eList, ap);
                    }
                }
                json_object_set_new(root, "AP list", eList);
            }
            else if(strncmp(param_ru, "/alarmsetting", 13) == 0)
            {
                unsigned char len;
                uint8_t deviceID = 1;
                AlarmSetting_t alarmsetting;

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }
                //param_ru += 11;

                if (strncmp(param_ru, "/1", 2) == 0) {
                    deviceID = 1;
                }
                else if (strncmp(param_ru, "/2", 2) == 0) {
                    deviceID = 2;
                }
                else if (strncmp(param_ru, "/3", 2) == 0) {
                    deviceID = 3;
                }

                xt_getalarmsetting_ipc(deviceID, &alarmsetting);
                json_object_set_new(root, "tempMax", json_integer(alarmsetting.tempmax));
                json_object_set_new(root, "tempMin", json_integer(alarmsetting.tempmin));
                json_object_set_new(root, "humiMax", json_integer(alarmsetting.humimax));
                json_object_set_new(root, "humiMin", json_integer(alarmsetting.humimin));
                json_object_set_new(root, "tempDeadband", json_integer(alarmsetting.tempdeadband));
                json_object_set_new(root, "humiDeadband", json_integer(alarmsetting.humideadband));
                
            }
            else if(strncmp(param_ru, "/aisetting", 10) == 0)
            {
                unsigned char len;
                uint8_t deviceID = 1;
                AISetting_t aisetting;

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }
                //param_ru += 11;

                if (strncmp(param_ru, "/1", 2) == 0) {
                    deviceID = 1;
                }
                else if (strncmp(param_ru, "/2", 2) == 0) {
                    deviceID = 2;
                }
                else if (strncmp(param_ru, "/3", 2) == 0) {
                    deviceID = 3;
                }

                xt_getaisetting_ipc(deviceID, &aisetting);
                json_object_set_new(root, "enable", json_boolean(aisetting.enable));
                json_object_set_new(root, "controlresponses", json_integer(aisetting.controlresponses));
                json_object_set_new(root, "learningspeed", json_integer(aisetting.learningspeed));
            }
            else if(strncmp(param_ru, "/aclist", 7) == 0)
            {
                json_error_t jerror;
                json_decref(root);
                root = json_load_file("/etc/t1/ACList.json", 0, &jerror);
                if(!root) {
                    printf("error aclist\n  ");
                    /* the error variable contains error information */
                }
            }
            else if(strncmp(param_ru, "/acbrand", 8) == 0)
            {
                unsigned char len;
                uint8_t deviceID = 1;
                ACBrand_t acbrand;

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }
                //param_ru += 11;

                if (strncmp(param_ru, "/1", 2) == 0) {
                    deviceID = 1;
                }
                else if (strncmp(param_ru, "/2", 2) == 0) {
                    deviceID = 2;
                }
                else if (strncmp(param_ru, "/3", 2) == 0) {
                    deviceID = 3;
                }

                xt_getacbrand_ipc(deviceID, &acbrand);
                json_object_set_new(root, "ac_brand", json_string(acbrand.ac_brand));
                json_object_set_new(root, "model_index", json_integer(acbrand.model_index));
            }
            else if(strncmp(param_ru, "/filelist", 9) == 0)
            {   
                json_error_t jerror;
                json_decref(root);
                root = json_load_file("/etc/t1/filelist.json", 0, &jerror);
                if(!root) {
                    printf("error aclist\n  ");
                    /* the error variable contains error information */
                }
            }
            else if(strncmp(param_ru, "/file", 5) == 0)
            {
                unsigned char len;
                char filedir_buffer[100];
                unsigned char buffer[XT_FCGI_MAX_CONTENT_LEN];
                uint16_t buffer_len = 0;
                FILE *ptr;

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }
                snprintf(filedir_buffer,100,  "/tmp%s", param_ru);

                if(strcmp(param_ru, "/sensordata.csv")==0)
                {
                    system("sqlite3 -header -csv /etc/t1/sensordata.db \"select * from sensordata where date= \'2019-05-01\' limit 30;\" > /tmp/temp.csv");
                    system("cut -d, -f2-6 /tmp/temp.csv > /tmp/sensordata.csv");
                }
                else if(strcmp(param_ru, "/acevent.csv")==0)
                {
                    system("sqlite3 -header -csv /etc/t1/sensordata.db \"select * from acevent where date= \'2019-05-01\' limit 30;\" > /tmp/temp.csv");
                    system("cut -d, -f2-99 /tmp/temp.csv > /tmp/acevent.csv");
                }
                else if(strcmp(param_ru, "/alarmevent.csv")==0)
                {
                    system("sqlite3 -header -csv /etc/t1/sensordata.db \"select * from alarmevent where date= \'2019-05-01\' limit 30;\" > /tmp/temp.csv");
                    system("cut -d, -f2-99 /tmp/temp.csv > /tmp/alarmevent.csv");
                }
                else if(strcmp(param_ru, "/aisetting.csv")==0)
                {
                    system("sqlite3 -header -csv /etc/t1/setting.db \"select * from aisetting;\" > /tmp/temp.csv");
                    system("cut -d, -f2-99 /tmp/temp.csv > /tmp/aisetting.csv");
                }
                else if(strcmp(param_ru, "/alarmsetting.csv")==0)
                {
                    system("sqlite3 -header -csv /etc/t1/setting.db \"select * from alarmsetting;\" > /tmp/temp.csv");
                    system("cut -d, -f2-99 /tmp/temp.csv > /tmp/alarmsetting.csv");
                }
                else if(strcmp(param_ru, "/schedule.csv")==0)
                {
                    system("sqlite3 -header -csv /etc/t1/schedule.db \"select * from schedule;\" > /tmp/temp.csv");
                    system("cut -d, -f2-99 /tmp/temp.csv > /tmp/schedule.csv");
                }

                ptr = fopen(filedir_buffer,"rb");
                if(ptr)
                {
                    buffer_len = fread(buffer,1,XT_FCGI_MAX_CONTENT_LEN,ptr); 
                }
                FCGX_PutStr("\r\n", 2, request.out);
                buffer[buffer_len] = 0;
                FCGX_PutStr((const char*)buffer,buffer_len, request.out);
                reply_json=0;
            }
            else if(strncmp(param_ru, "/irrxlog", 8) == 0)
            {
                R_CONTROLCODE_LOG_t irlog[R_BUFFER_SIZE];
                DateTime_t start;
                DateTime_t end;
                uint16_t count;
                json_t *eList = json_array();
                uint8_t len;

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }
                //param_ru += 11;

                count = xt_getir_log(irlog, R_BUFFER_SIZE);
                //put into json
                for(int i=0; i<count; i++)
                {
                    json_t *device = json_object();
                    json_t *rawir_array = json_array();

                    json_object_set_new(device, "Date", json_string((const char *)irlog[i].dateTime.date));
                    json_object_set_new(device, "Time", json_string((const char *)irlog[i].dateTime.time));
                    json_object_set_new(device, "IR_len", json_integer(irlog[i].code.count));
                    for(int j=0; j<irlog[i].code.count; j++)
                    {
                        json_array_append_new(rawir_array, json_integer(irlog[i].code.usec[j]));
                    }
                    json_object_set_new(device, "IR_data", rawir_array);
                    json_array_append_new(eList, device);
                }

                json_object_set_new(root, "RX IR Log", eList);
            }
            else if(strncmp(param_ru, "/irrxlast", 9) == 0)
            {
                R_CONTROLCODE_LOG_t irlog[R_BUFFER_SIZE];
                DateTime_t start;
                DateTime_t end;
                uint16_t count;
                json_t *eList = json_array();
                uint8_t len;

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }
                //param_ru += 11;
                count = xt_getlastir_log(irlog);
                //put into json
                for(int i=0; i<count; i++)
                {
                    json_t *device = json_object();
                    json_t *rawir_array = json_array();

                    json_object_set_new(device, "Date", json_string((const char *)irlog[i].dateTime.date));
                    json_object_set_new(device, "Time", json_string((const char *)irlog[i].dateTime.time));
                    json_object_set_new(device, "IR_len", json_integer(irlog[i].code.count));
                    for(int j=0; j<irlog[i].code.count; j++)
                    {
                        json_array_append_new(rawir_array, json_integer(irlog[i].code.usec[j]));
                    }
                    json_object_set_new(device, "IR_data", rawir_array);
                    json_array_append_new(eList, device);
                }

                json_object_set_new(root, "RX IR Log", eList);
            }
            else if(strncmp(param_ru, "/irrxdate", 9) == 0)
            {
                R_CONTROLCODE_LOG_t irlog[R_BUFFER_SIZE];
                DateTime_t start;
                uint16_t count;
                json_t *eList = json_array();
                uint8_t len;

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }
                //param_ru += 11;

                //clear all date time 
                memset(&start, 0x00, sizeof(DateTime_t));

                if(getdirectorylength(param_ru,&len))
                {
                    //date retrieval
                    //TODO:validate date
                    if(len == DATE_LENGTH)
                    {
                        //perform cpy
                        memcpy(start.date, param_ru+1, DATE_LENGTH - 1);
                        start.date[DATE_LENGTH-1] = 0;
                    }
                    param_ru += len;
                }

                if(getdirectorylength(param_ru,&len))
                {
                    //date retrieval
                    //TODO:validate time
                    if(len == TIME_LENGTH)
                    {
                        //perform cpy
                        memcpy(start.time, param_ru+1, TIME_LENGTH - 1);
                        start.time[TIME_LENGTH-1] = 0;
                    }
                    param_ru += len;
                }

                count = xt_getdateir_log(&start, irlog, R_BUFFER_SIZE);
                //put into json
                for(int i=0; i<count; i++)
                {
                    json_t *device = json_object();
                    json_t *rawir_array = json_array();

                    json_object_set_new(device, "Date", json_string((const char *)irlog[i].dateTime.date));
                    json_object_set_new(device, "Time", json_string((const char *)irlog[i].dateTime.time));
                    json_object_set_new(device, "IR_len", json_integer(irlog[i].code.count));
                    for(int j=0; j<irlog[i].code.count; j++)
                    {
                        json_array_append_new(rawir_array, json_integer(irlog[i].code.usec[j]));
                    }
                    json_object_set_new(device, "IR_data", rawir_array);
                    json_array_append_new(eList, device);
                }

                json_object_set_new(root, "RX IR Log", eList);
            }
            else if(strncmp(param_ru, "/license", 8) == 0)
            {
                bool licensed;
                licensed = confirmLicense();
                json_object_set_new(root, "licensed", json_boolean(licensed));
                if(licensed)
                {
                    json_object_set_new(root, "license_type", json_string((const char *)"STANDARD"));
                }
                else
                {
                    json_object_set_new(root, "license_type", json_string((const char *)"NULL"));                    
                }

                if(xt_device_mac(macstr) <= 0)
                {
                    memcpy(macstr, "NOT FOUND", sizeof("NOT FOUND"));
                }

                json_object_set_new(root, "mac", json_string((const char *)macstr));
            }
#ifdef FACTORY_BUILD
            else if(strncmp(param_ru, "/getnewid", 9) == 0)
            {
                char zigbeeUID[30];
                renewUID();
                createLicenseInfo();
                getZigBeeEUID(zigbeeUID);
                json_object_set_new(root, "deviceUID", json_string((const char *)zigbeeUID));

            }
            else if(strncmp(param_ru, "/hwtesting", 10) == 0)
            {
                bool result;
                float tempvalue;
                float humivalue;

                //perfrom IR testing
                result = 1;
                json_object_set_new(root, "IR", json_string(testIR()?"OK":"NG"));

                //perfrom Sensor Reading
                xt_readbuildsensor_ipc(1, &tempvalue, &humivalue);
                json_object_set_new(root, "SensorTemp", json_real(tempvalue));
                json_object_set_new(root, "SensorHumi", json_real(humivalue));

                //perform Zigbee testing

                //perform IP retrieval
                if(xt_device_ip(macstr) != 0)
                {
                    memcpy(macstr, "NOT FOUND", sizeof("NOT FOUND"));
                }
                json_object_set_new(root, "IP", json_string((const char *)macstr));

                //perfrom MAC retrieval
                if(xt_device_mac(macstr) <= 0)
                {
                    memcpy(macstr, "NOT FOUND", sizeof("NOT FOUND"));
                }
                json_object_set_new(root, "MAC", json_string((const char *)macstr));
                
                //performm FW retrieval
                if(xt_get_t1_version(macstr) != 0)
                {
                    memcpy(macstr, "NOT FOUND", sizeof("NOT FOUND"));
                }
                json_object_set_new(root, "FW", json_string((const char *)macstr));

                //perform chipID retrieval
                if(xt_get_chip_id(macstr) != 0)
                {
                    memcpy(macstr, "NOT FOUND", sizeof("NOT FOUND"));
                }
                json_object_set_new(root, "CHIPID", json_string((const char *)macstr));

                //perform SPIID retrieval
                if(xt_get_spi_id(macstr) != 0)
                {
                    memcpy(macstr, "NOT FOUND", sizeof("NOT FOUND"));
                }
                json_object_set_new(root, "SPIID", json_string((const char *)macstr));
            }
#endif            
        }
        else if(strcmp(param_method, "PATCH") == 0)
        {
            char *buf = NULL;
            uint16_t buf_len;
            xt_fcgi_get_content_len(&request, &buf, &buf_len);

            if (strncmp(param_ru, "/AC", 3) == 0)
            {
                json_error_t jerror;
                bool updated = 0;
                json_t *content = json_loads(buf, 0, &jerror);
                uint8_t deviceID = 1;
                ACSetting_t acSetting;
				const char *reasonVal = NULL;
                unsigned char len;
                bool turnOnOffVal;
                bool ACStatus;
                uint8_t eventSrcVal = 0;

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }
                //param_ru += 11;

                if (strncmp(param_ru, "/1", 2) == 0) {
                    deviceID = 1;
                }
                else if (strncmp(param_ru, "/2", 2) == 0) {
                    deviceID = 2;
                }
                else if (strncmp(param_ru, "/3", 2) == 0) {
                    deviceID = 3;
                }

                if (content)
                {
                    if (json_is_object(content))
                    {
                        json_t *temp = json_object_get(content, "temp");
                        json_t *fanspeed = json_object_get(content, "fanspeed");
                        json_t *mode = json_object_get(content, "mode");
                        json_t *isSwing = json_object_get(content, "isSwing");
                        json_t *turnOnOFF = json_object_get(content, "turnOnOFF");
                        json_t *eventSrc = json_object_get(content, "eventSrc");
                        json_t *reason = json_object_get(content, "reason");
                        xt_getac_ipc(deviceID, &acSetting, &ACStatus);

                        if(temp && json_is_integer(temp))
                        {
                            acSetting.temp = json_integer_value(temp);
                            updated = 1;
                        }

                        if(fanspeed && json_is_integer(fanspeed))
                        {
                            acSetting.fanspeed = json_integer_value(fanspeed);
                            updated = 1;
                        }

                        if(mode && json_is_integer(mode))
                        {
                            acSetting.mode = (E_AC_MODE)json_integer_value(mode);
                            updated = 1;
                        }

                        if(isSwing && json_is_boolean(isSwing))
                        {
                            acSetting.isSwing = (1 == json_boolean_value(isSwing));
                            updated = 1;
                        }

                        if(turnOnOFF && json_is_boolean(turnOnOFF))
                        {
                            turnOnOffVal = (1 == json_boolean_value(turnOnOFF));
                            updated = 1;
                        }
                        else
                        {
                            turnOnOffVal = ACStatus;
                        }

                        if(eventSrc && json_integer_value(eventSrc))
                        {
                            eventSrcVal = json_integer_value(eventSrc);
                            updated = 1;
                        }

                        if(reason && json_is_string(reason))
                        {
                            reasonVal = json_string_value(reason);
                        }
                        if(reasonVal == NULL)
                        {
                            reasonVal = "";
                        }

                        if(updated)
                        {
							//'reasonVal' pointer validity depends on 'reason' JSON object. perhaps dangerous?
                            //SetAC(deviceID, &acSetting, turnOnOffVal, eventSrcVal, reasonVal);
                            xt_accontrol_ipc(deviceID, &acSetting, turnOnOffVal, eventSrcVal,reasonVal);
                        }
                    }
                }
                //GetAC(deviceID, &acSetting);
                json_object_set_new(root, "temp", json_integer(acSetting.temp));
                json_object_set_new(root, "fanspeed", json_integer(acSetting.fanspeed));
                json_object_set_new(root, "mode", json_integer(acSetting.mode));
                json_object_set_new(root, "isSwing", json_boolean(acSetting.isSwing));
            }
            else if(strncmp(param_ru, "/devices", 8) == 0)
            {   
                json_error_t jerror;
                json_t *content = json_loads(buf, 0, &jerror);
                json_t *device_list = json_object_get(content, "Device List");
                json_t *element = 0;
                bool labelvalid=0;
                char *jstr;
                if(json_is_array(device_list))
                {
                    json_array_foreach(device_list, i, element)
                    {
                        const char *labelVal = NULL;
                        uint8_t indexVal;
                        json_t *label = json_object_get(element, "label");
                        json_t *index = json_object_get(element, "index");
                        if(index && json_is_integer(index))
                        {
                            indexVal = json_integer_value(index);
                            if(label && json_is_string(label))
                            {
                                labelVal = json_string_value(label);
                                xt_setdevicelabel_ipc(indexVal, (char*)labelVal);
                                //memcpy(HardcodedDevice.device[i].deviceName, labelVal, strlen(labelVal));
                                //HardcodedDevice.device[i].deviceName[strlen(labelVal)] = 0;
                                labelvalid=1;
                            }
                        }
                        if(labelvalid)
                        {
                            json_object_set_new(root, "status", json_string("OK"));
                            json_object_set_new(content, "dataType", json_string("SetDevices"));
                            jstr = json_dumps(content, JSON_INDENT(3));
                            xt_mqttbuffer_ipc(jstr, strlen(jstr));
                        }
                        else
                        {
                            json_object_set_new(root, "status", json_string("NG"));
                        }
                        
                    }
                }
            }
            else if(strncmp(param_ru, "/SyncACState", 12) == 0)
            {
                uint8_t deviceID = 1;
                bool newACStatus = 0;
                uint8_t len;
                unsigned char eventSrc=0;

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }
                //param_ru += 11;

                if (strncmp(param_ru, "/1", 2) == 0) {
                    deviceID = 1;
                }
                else if (strncmp(param_ru, "/2", 2) == 0) {
                    deviceID = 2;
                }
                else if (strncmp(param_ru, "/3", 2) == 0) {
                    deviceID = 3;
                }

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }

                if(getdirectorylength(param_ru,&len))
                {
                    if(len == 2)
                    {
                        newACStatus = (*(param_ru+1)) & 0x01;
                    }
                    param_ru += len;
                }

                if(getdirectorylength(param_ru,&len))
                {
                    eventSrc = asciitohex(param_ru+1, len);
                }

                xt_acsync_ipc(deviceID, newACStatus, eventSrc);
            }
            else if(strncmp(param_ru, "/setwifi-", 9) == 0) {
                printf("entering setwifi\n");
                const char *ssid_buff = NULL;
                const char *password_buff = NULL;
                json_object_set_new(root, "status", json_string("OK"));

                json_error_t jerror;
                json_t *content = json_loads(buf, 0, &jerror);
                json_t *temp[10];
                for (int i = 0; i < 9; i++)
                {
                    temp[i] = NULL;
                }
                temp[0] = json_object_get(content, "ssid");
                temp[1] = json_object_get(content, "password");
                temp[2] = json_object_get(content, "mode");
                temp[3] = json_object_get(content, "staticIP");
                temp[4] = json_object_get(content, "subnetMask");
                temp[5] = json_object_get(content, "defaultGateway");
                temp[6] = json_object_get(content, "broadcastAddress");
                temp[7] = json_object_get(content, "defaultdns");
                temp[8] = json_object_get(content, "hiddenSSID");

                for (int i = 0; i < 9; i++)
                {
                    printf("pointer is %p", temp[i]);
                    if (temp[i]) printf("found %d: %s\n", i, json_string_value(temp[i]));
                }

                if (temp[0] && json_is_string(temp[0])) {
                    ssid_buff = json_string_value(temp[0]);
                    printf("found ssid: %s\n", ssid_buff);

                }
                if (temp[1] && json_is_string(temp[1])) {
                    password_buff = json_string_value(temp[1]);
                    printf("found password: %s\n", password_buff);

                }
            }
            else if(strncmp(param_ru, "/setwifiadv", 11) == 0)
            {
                uint8_t deviceID = 1;
                json_t *element;
                int len = 0;
                const char *ssid_buff = NULL;
                const char *password_buff = NULL;
                const char *mode_buff = NULL;
                const char *staticIP_buff = NULL;
                const char *subnetMask_buff = NULL;
                const char *defaultGateway_buff = NULL;
                const char *broadcastAddress_buff = NULL;
                const char *defaultdns_buff = NULL;
                char ip[16];
                char mask[16];
                char gw[16];
                char bc[16];
                char dns[16];
                bool bHiddenSSID = false;
                int octets[4];

                bool setWifi = false;
                bool setStatic = false;

                bool valid = false;

                json_error_t jerror;
                json_t *content = json_loads(buf, 0, &jerror);

                json_t *ssid = json_object_get(content, "ssid");
                json_t *password = json_object_get(content, "password");
                json_t *mode = json_object_get(content, "mode");
                json_t *staticIP = json_object_get(content, "staticIP");
                json_t *subnetMask = json_object_get(content, "subnetMask");
                json_t *defaultGateway = json_object_get(content, "defaultGateway");
                json_t *broadcastAddress = json_object_get(content, "broadcastAddress");
                json_t *defaultdns = json_object_get(content, "defaultdns");
                json_t *hiddenSSID = json_object_get(content, "hiddenSSID");

                if (ssid && json_is_string(ssid)) {
                    ssid_buff = json_string_value(ssid);
                    printf("found ssid: %s\n", ssid_buff);
                    if(hiddenSSID && json_is_boolean(hiddenSSID))
                    {
                        bHiddenSSID = json_boolean_value(hiddenSSID);
                    }

                    if(password && json_is_string(password))
                    {
                        password_buff = json_string_value(password);
                        printf("found password: %s\n", password_buff);
                        if (!bHiddenSSID) {         // normal wifi needs only SSID and password
                            setWifi = true;
                        }

                        if(mode && json_is_string(mode))
                        {
                            mode_buff = json_string_value(mode);

                            setWifi = true;
                        }
                        else  // get mode from ap scan.
                        {
                            printf("encryption not supplied, scanning to find encryption...\n");
                            T_AP_DATA ap_list[MAX_AP_LIST];
                            int count = 0;
                            //count = xt_get_wifimon_ap_list(ap_list);
                            //printf("count of ap_scan from wifimon: %d\n", count);
                            if (count <= 0) {
                                count = xt_get_ap_list(ap_list);
                                printf("count of ap_scan from xt_device_info: %d\n", count);
                            }
                            if(count>0) {
                                for (int i = 0; i < count; i++) {
                                    if(strcmp(ap_list[i].ssid, ssid_buff) == 0) {
                                        strcpy(wifi.mode, ap_list[i].encryption);
                                        printf("encryption found. [%s]\n", mode_buff);
                                        setWifi = true;
                                    }
                                }
                            }
                        }
                    }
                    if(mode && json_is_string(mode)) {
                        mode_buff = json_string_value(mode);
                        printf("found mode: %s\n", mode_buff);
                    }
                }
                printf("staticIP: %p\n", staticIP);
                printf("setStatic: %d\n", setStatic);
                printf("setWifi: %d\n", setWifi);

                if(staticIP && json_is_string(staticIP))
                {
                    staticIP_buff = json_string_value(staticIP);
                    if (ip_to_uint(staticIP_buff)) {
                        ip_to_string(ip, ip_to_uint(staticIP_buff));
                        setStatic = true;
                    }
                    if (setStatic)
                    {
                        uint32_t tmp_val = 0;
                        uint32_t tmp_gw = 0;

                        if(subnetMask && json_is_string(subnetMask))
                        {
                            subnetMask_buff = json_string_value(subnetMask);
                        }
                        if(defaultGateway && json_is_string(defaultGateway))
                        {
                            defaultGateway_buff = json_string_value(defaultGateway);
                        }
                        if(broadcastAddress && json_is_string(broadcastAddress))
                        {
                            broadcastAddress_buff = json_string_value(broadcastAddress);
                        }
                        if(defaultdns && json_is_string(defaultdns))
                        {
                            defaultdns_buff = json_string_value(defaultdns);
                        }

                        tmp_val = subnetMask_buff ? ip_to_uint(subnetMask_buff) : 0;
                        ip_to_string(mask, tmp_val ? tmp_val : 0xFFFFFF00);

                        tmp_gw = defaultGateway_buff ? ip_to_uint(defaultGateway_buff) : 0;
                        tmp_gw = tmp_gw ? tmp_gw : (ip_to_uint(ip) & 0xFFFFFF00) | 0x1;
                        ip_to_string(gw, tmp_gw);

                        tmp_val = broadcastAddress_buff ? ip_to_uint(broadcastAddress_buff) : 0;
                        ip_to_string(bc, tmp_val ? tmp_val : (ip_to_uint(ip) & 0xFFFFFF00) | 0xFF);

                        tmp_val = defaultdns_buff ? ip_to_uint(defaultdns_buff) : 0;
                        ip_to_string(dns, tmp_val ? tmp_val : ip_to_uint(gw));
                    }
                }


                if (setStatic) {
                    strncpy(wifi.staticIP, ip, 15);
                    strncpy(wifi.subnetMask, mask, 15);
                    strncpy(wifi.defaultGateway, gw, 15);
                    strncpy(wifi.broadcastAddress, bc, 15);
                    strncpy(wifi.defaultdns, dns, 15);
                    wifi.setstatic = true;

                    json_object_set_new(root, "status", json_string("OK"));
                } else {
                    wifi.setdhcp = true;
                }

                if (setWifi) {
                    wifi.setwifi = true;
                    if (ssid_buff && strlen(ssid_buff)) strncpy(wifi.ssid, ssid_buff, 63);
                    if (password_buff && strlen(password_buff)) strncpy(wifi.password, password_buff, 63);
                    if (mode_buff && strlen(mode_buff)) strncpy(wifi.mode, mode_buff, 9);
                    wifi.hiddenSSID = bHiddenSSID;
                }

                if(!setWifi & !setStatic) {
                    json_object_set_new(root, "type", json_string("Not Valid"));
                    json_object_set_new(root, "status", json_string("NG4"));
                }
                else {
                    json_object_set_new(root, "status", json_string("OK"));
                }

            }
            else if(strncmp(param_ru, "/setwifi", 8) == 0)
            {
                uint8_t deviceID = 1;

                char ssidbuffer[50];
                char pwbuffer[50];
                uint8_t len;
                bool valid = 1;

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }
                //param_ru += 11;

                if (strncmp(param_ru, "/1", 2) == 0) {
                    deviceID = 1;
                }
                else if (strncmp(param_ru, "/2", 2) == 0) {
                    deviceID = 2;
                }
                else if (strncmp(param_ru, "/3", 2) == 0) {
                    deviceID = 3;
                }

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }

                ssidbuffer[0] = 0;
                if(getdirectorylength(param_ru, &len))
                {
                    memcpy(ssidbuffer, param_ru+1, len-1);
                    ssidbuffer[len-1] = 0;
                    param_ru += len;
                }
                else
                {
                    valid = 0;
                }

                pwbuffer[0] = 0;
                if(getdirectorylength(param_ru, &len))
                {
                    memcpy(pwbuffer, param_ru+1, len-1);
                    pwbuffer[len-1] = 0;
                    param_ru += len;
                }
                else
                {
                    valid = 0;
                }

                if(valid)
                {
                    json_object_set_new(root, "status", json_string("OK"));
                }
                else
                {
                    json_object_set_new(root, "status", json_string("NG"));
                }

            }
            else if(strncmp(param_ru, "/schedule", 9) == 0)
            {
                json_error_t jerror;
                json_t *content = json_loads(buf, 0, &jerror);
                json_t *scheSetting = json_object_get(content, "Schedule setting");
                json_t *element = 0;
                int i = 0;
                char *jstr;
                if(json_is_array(scheSetting))
                {
                    json_array_foreach(scheSetting, i, element)
                    {
                        xt_schedule_t schedule_setting;
                        uint8_t indexVal = 0;
                        const char dummydate[]="0000-00-00";
                        const char dummytime[]="00:00:00";
                        const char *DateVal = NULL;
                        const char *TimeVal = NULL;

                        json_t *temp = json_object_get(element, "temp");
                        json_t *fanspeed = json_object_get(element, "fanspeed");
                        json_t *mode = json_object_get(element, "mode");
                        json_t *isSwing = json_object_get(element, "isSwing");
                        json_t *turnOnOFF = json_object_get(element, "turnOnOFF");
                        json_t *deviceId = json_object_get(element, "deviceId");
                        json_t *index = json_object_get(element, "index");
                        json_t *Date = json_object_get(element, "Date");
                        json_t *Time = json_object_get(element, "Time");
                        json_t *days = json_object_get(element, "days");       
                        json_t *enable = json_object_get(element, "enable");
                        json_t *notify = json_object_get(element, "notify");
                        json_t *aienabled = json_object_get(element, "aienabled");

                        if(temp && json_is_integer(temp))
                        {
                            schedule_setting.acSetting.ACSetting.temp = json_integer_value(temp);
                        }

                        if(fanspeed && json_is_integer(fanspeed))
                        {
                            schedule_setting.acSetting.ACSetting.fanspeed = json_integer_value(fanspeed);
                        }

                        if(mode && json_is_integer(mode))
                        {
                            schedule_setting.acSetting.ACSetting.mode = (E_AC_MODE)json_integer_value(mode);
                        }

                        if(isSwing && json_is_boolean(isSwing))
                        {
                            schedule_setting.acSetting.ACSetting.isSwing = (1 == json_boolean_value(isSwing));
                        }

                        if(turnOnOFF && json_is_boolean(turnOnOFF))
                        {
                            schedule_setting.acSetting.acStatus = (1 == json_boolean_value(turnOnOFF));
                        }

                        if(enable && json_is_boolean(enable))
                        {
                            schedule_setting.enabled = (1 == json_boolean_value(enable));
                        }

                        if(notify && json_is_boolean(notify))
                        {
                            schedule_setting.notify = (1 == json_boolean_value(notify));
                        }

                        if(days && json_is_integer(days))
                        {
                            schedule_setting.days = json_integer_value(days);
                        }                 
                        
                        if(deviceId && json_is_integer(deviceId))
                        {
                            schedule_setting.acSetting.deviceID = json_integer_value(deviceId);
                        }

                        if(Date && json_is_string(Date))
                        {
                            DateVal = json_string_value(Date);
                        }
                        if(DateVal == NULL)
                        {
                            DateVal = dummydate;
                        }

                        memcpy(schedule_setting.specific_date, DateVal, DATE_LENGTH);

                        if(Time && json_is_string(Time))
                        {
                            TimeVal = json_string_value(Time);
                        }
                        if(TimeVal == NULL)
                        {
                            TimeVal = dummytime;
                        }

                        sscanf(TimeVal, "%hhu:%hhu", &schedule_setting.hours, &schedule_setting.minutes);

                        if(index && json_is_integer(index))
                        {
                            indexVal = json_integer_value(index);
                        }

                        if(aienabled && json_is_boolean(aienabled))
                        {
                            schedule_setting.aienabled = json_boolean_value(aienabled);
                        }

                        xt_updateschedule_ipc(indexVal, &schedule_setting);
                    }
                    json_object_set_new(content, "dataType", json_string("SetSchedule"));
                    jstr = json_dumps(content, JSON_INDENT(3));
                    xt_mqttbuffer_ipc(jstr, strlen(jstr));
                    free(jstr);
                }
            }
            else if(strncmp(param_ru, "/alarmsetting", 13) == 0)
            {
                unsigned char len;
                uint8_t deviceID = 1;
                AlarmSetting_t alarmsetting;
                json_error_t jerror;
                json_t *content = json_loads(buf, 0, &jerror);
                char *jstr;

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }
                //param_ru += 11;

                if (strncmp(param_ru, "/1", 2) == 0) {
                    deviceID = 1;
                }
                else if (strncmp(param_ru, "/2", 2) == 0) {
                    deviceID = 2;
                }
                else if (strncmp(param_ru, "/3", 2) == 0) {
                    deviceID = 3;
                }

                if (content)
                {
                    if (json_is_object(content))
                    {
                        json_t *tempMax = json_object_get(content, "tempMax");
                        json_t *tempMin = json_object_get(content, "tempMin");
                        json_t *humiMax = json_object_get(content, "humiMax");
                        json_t *humiMin = json_object_get(content, "humiMin");
                        json_t *tempdeadband = json_object_get(content, "tempDeadband");
                        json_t *humideadband = json_object_get(content, "humiDeadband");

                        xt_getalarmsetting_ipc(deviceID, &alarmsetting);

                        if(tempMax && json_is_integer(tempMax))
                        {
                            alarmsetting.tempmax = json_integer_value(tempMax);
                        }

                        if(tempMin && json_is_integer(tempMin))
                        {
                            alarmsetting.tempmin = json_integer_value(tempMin);
                        }

                        if(humiMax && json_is_integer(humiMax))
                        {
                            alarmsetting.humimax = json_integer_value(humiMax);
                        }

                        if(humiMin && json_is_integer(humiMin))
                        {
                            alarmsetting.humimin = json_integer_value(humiMin);
                        }

                        if(tempdeadband && json_is_integer(tempdeadband))
                        {
                            alarmsetting.tempdeadband = json_integer_value(tempdeadband);
                        }

                        if(humideadband && json_is_integer(humideadband))
                        {
                            alarmsetting.humideadband = json_integer_value(humideadband);
                        }

                        xt_setalarmsetting_ipc(deviceID, &alarmsetting);
                        xt_updatesensoralarm_ipc(deviceID, &alarmsetting);

                    }
                }
                
                json_object_set_new(root, "status", json_string("OK"));
                json_object_set_new(content, "dataType", json_string("SetAlarmSetting"));
                json_object_set_new(content, "localID", json_integer(deviceID));

                jstr = json_dumps(content, JSON_INDENT(3));
                xt_mqttbuffer_ipc(jstr, strlen(jstr));
            }
            else if(strncmp(param_ru, "/aisetting", 10) == 0)
            {
                unsigned char len;
                uint8_t deviceID = 1;
                AISetting_t aisetting;
                json_error_t jerror;
                json_t *content = json_loads(buf, 0, &jerror);
                char *jstr;

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }
                //param_ru += 11;

                if (strncmp(param_ru, "/1", 2) == 0) {
                    deviceID = 1;
                }
                else if (strncmp(param_ru, "/2", 2) == 0) {
                    deviceID = 2;
                }
                else if (strncmp(param_ru, "/3", 2) == 0) {
                    deviceID = 3;
                }

                if (content)
                {
                    if (json_is_object(content))
                    {
                        json_t *enable = json_object_get(content, "enable");
                        json_t *controlresponses = json_object_get(content, "controlresponses");
                        json_t *learningspeed = json_object_get(content, "learningspeed");

                        xt_getaisetting_ipc(deviceID, &aisetting);
                        printf("got patch%d %d\n", enable, json_is_boolean(enable));
                        if(enable && json_is_boolean(enable))
                        {
                            aisetting.enable = json_boolean_value(enable);
                            printf("got enable%d\n", aisetting.enable);
                        }

                        if(controlresponses && json_is_integer(controlresponses))
                        {
                            aisetting.controlresponses = json_integer_value(controlresponses);
                        }

                        if(learningspeed && json_is_integer(learningspeed))
                        {
                            aisetting.learningspeed = json_integer_value(learningspeed);
                        }

                        xt_setaisetting_ipc(deviceID, &aisetting);
                    }
                }
                json_object_set_new(root, "status", json_string("OK"));
                json_object_set_new(content, "dataType", json_string("SetAISetting"));
                json_object_set_new(content, "localID", json_integer(deviceID));
                jstr = json_dumps(content, JSON_INDENT(3));
                xt_mqttbuffer_ipc(jstr, strlen(jstr));
            }
            // else if(strncmp(param_ru, "/setwifiadv", 11) == 0)
            // {
            //     json_object_set_new(root, "status", json_string("OK"));
            // }
            else if(strncmp(param_ru, "/acbrand", 8) == 0)
            {
                unsigned char len;
                uint8_t deviceID = 1;
                ACBrand_t acbrand;
                json_error_t jerror;
                json_t *content = json_loads(buf, 0, &jerror);

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }
                //param_ru += 11;

                if (strncmp(param_ru, "/1", 2) == 0) {
                    deviceID = 1;
                }
                else if (strncmp(param_ru, "/2", 2) == 0) {
                    deviceID = 2;
                }
                else if (strncmp(param_ru, "/3", 2) == 0) {
                    deviceID = 3;
                }

                if (content)
                {
                    if (json_is_object(content))
                    {
                        json_t *ac_brand = json_object_get(content, "ac_brand");
                        json_t *model_index = json_object_get(content, "model_index");

                        xt_getacbrand_ipc(deviceID, &acbrand);

                        if(ac_brand && json_is_string(ac_brand))
                        {
                            memcpy(acbrand.ac_brand, json_string_value(ac_brand), strlen(json_string_value(ac_brand)));
                            acbrand.ac_brand[strlen(json_string_value(ac_brand))] = 0;
                        }

                        if(model_index && json_is_integer(model_index))
                        {
                            acbrand.model_index = json_integer_value(model_index);
                        }

                        acbrand.isLocal = 1;

                        xt_setacbrand_ipc(deviceID, &acbrand);
                    }
                }
            }
            else if(strncmp(param_ru, "/xt_update", strlen("/xt_update")) == 0)
            {
                upgrade_type type = UNKNOWN;
                char file[128];
                char url[128];
                char type_str[16];
                int result = 1;
                json_error_t jerror;
                json_t *content = json_loads(buf, 0, &jerror);
                char *jstr;

                memset(url, 0x00, 128);
                printf("testing xt_update\n");
                if (content)
                {
                    if (json_is_object(content))
                    {
                        json_t *url_json = json_object_get(content, "url");
                        json_t *file_json = json_object_get(content, "file");
                        json_t *type_json = json_object_get(content, "type");

                        if(url_json && json_is_string(url_json)) strcpy(url, json_string_value(url_json));
                        if(file_json && json_is_string(file_json)) strcpy(file, json_string_value(file_json));
                        if(type_json && json_is_string(type_json)) {
                            strcpy(type_str, json_string_value(type_json));
                            if(strncmp(type_str, "sysupgrade", strlen("sysupgrade")) == 0) type = SYSUPGRADE_FILE;
                            else if(strncmp(type_str, "opkg", strlen("opkg")) == 0) type = OPKG_FILE;
                            else if(strncmp(type_str, "zigbee_chip", strlen("zigbee_chip")) == 0) type = ZIGBEE_FW;
                            else type = UNKNOWN;
                        }

                        if(strlen(url) > 5 && strlen(file) >> 1) {
                            result = xt_wget_file(url, file);
                            if (!result) {
                                if(type == SYSUPGRADE_FILE) {
                                    result = xt_sysupgrade_untar(file);
                                    if (!result) {
                                        printf("would sysupgrade now");
                                        //result = xt_sysupgrade_install(tmpfile);
                                    }
                                    else json_object_set_new(root, "error", json_string("md5sum or version incorrect."));
                                }
                                else if (type == OPKG_FILE) {
                                    result = xt_opkg_untar(file);
                                    if (!result) {
                                        printf("would opkg install now");
                                        //result = xt_opkg_install(tmpfile)
                                    }
                                    else json_object_set_new(root, "error", json_string("md5sum or version incorrect."));
                                }
                                else if (type == ZIGBEE_FW) {
                                    result = xt_zigbeefw_untar(file);
                                    if (!result) {
                                        printf("would upgrade zigbee now");
                                        //result = 0;
                                        //xt_perform_zigbeefw_upgrade();
                                    }
                                    else json_object_set_new(root, "error", json_string("md5sum or version incorrect."));
                                }
                                else {
                                    result = 1;
                                    json_object_set_new(root, "error", json_string("unknow file type."));
                                }
                            }
                            else
                            {
                                json_object_set_new(root, "error", json_string("failed to wget file."));
                            }
                            printf("wget file result: %d\n", result);
                        }
                        printf("xt_update route called. url [%s] file [%s] type [%d]\n", url, file, type);
                    }
                }

                if(result == 0) {
                    json_object_set_new(root, "status", json_string("OK"));
                }
                else {
                    json_object_set_new(root, "status", json_string("NG4"));
                }
            }

            else if(strncmp(param_ru, "/xt_tar_backup", strlen("/xt_tar_backup")) == 0)
            {
                char files[1024];

                json_error_t jerror;
                json_t *content = json_loads(buf, 0, &jerror);
                char *jstr;

                printf("testing xt_tar_backup\n");
                if (content)
                {
                    if (json_is_object(content))
                    {
                        json_t *files_json = json_object_get(content, "files");


                        if(files_json && json_is_string(files_json)) strcpy(files, json_string_value(files_json));
                        printf("xt_tar_backup route called. files [%s]\n", files);
                        int result = xt_tar_backup(files);
                        if(result == 0) {
                            json_object_set_new(root, "status", json_string("OK"));
                        }
                        else {
                            json_object_set_new(root, "status", json_string("NG4"));
                        }
                    }
                }
            }
            else if(strncmp(param_ru, "/xt_tar_restore", strlen("/xt_tar_restore")) == 0)
            {
                char file[128];
                char url[128];
                int result = 1;
                json_error_t jerror;
                json_t *content = json_loads(buf, 0, &jerror);
                char *jstr;

                printf("testing xt_tar_restore\n");
                if (content)
                {
                    if (json_is_object(content))
                    {
                        json_t *file_json = json_object_get(content, "file");
                        json_t *url_json = json_object_get(content, "url");

                        if(url_json && json_is_string(url_json)) strcpy(url, json_string_value(url_json));
                        if(file_json && json_is_string(file_json)) strcpy(file, json_string_value(file_json));
                        printf("xt_tar_backup route called. file [%s]\n", file);
                        result = xt_wget_file(url, file);
                        if (!result) {
                        result = xt_untar_restore(file);
                        }
                        else {
                            json_object_set_new(root, "error", json_string("Unable to download file."));
                        }
                        if(result == 0) {
                            json_object_set_new(root, "status", json_string("OK"));
                        }
                        else {
                            json_object_set_new(root, "status", json_string("NG4"));
                        }
                    }
                }
            }
            else if (strncmp(param_ru, "/file", 5) == 0)
            {
                unsigned char len;                    
                int i;
                FILE * pFile;
                char filedir_buffer[100];
                char tablename[100];
                char* endptr;

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }
                snprintf(filedir_buffer,100,  "/tmp/%s", param_ru);

                pFile = fopen (filedir_buffer,"w");
                if (pFile!=NULL)
                {
                    fwrite(buf, 1 /*sizeof(short)*/, buf_len /*20/2*/, pFile);
                    //fputs ("fopen example",pFile);
                    fclose (pFile);
                }

                if((endptr = strstr(param_ru, ".csv"))!= NULL)
                {
                    //printf("tempptr: %x, endptr: %x\r\n", tempptr, endptr);
                    memcpy(tablename, param_ru, endptr-param_ru);
                    tablename[endptr-param_ru] = 0;
                    xt_importcsv_ipc(tablename);
                }

                json_object_set_new(root, "status", json_string("OK"));

                /*
                printf("recevied file:%d\n", buf_len);
                for(i=0; i<buf_len; i++)
                {
                    printf("%02x ",buf[i]);
                }
                printf("\n");
                */
            }
            else if (strncmp(param_ru, "/license", 8) == 0)
            {
                char temp_mac[6];
                json_error_t jerror;
                json_t *content = json_loads(buf, 0, &jerror);
                char *jstr;
                bool licensed;
                bool invalid_mac = 0;
                int output_file; // file descriptor for output_file

                if ((content)&&(json_is_object(content)))
                {                   
                    json_t *license = json_object_get(content, "license");
                    json_t *mac = json_object_get(content, "mac");

                    if(license && json_is_string(license))
                    {
                        output_file = open("/etc/t1/.license", O_WRONLY | O_CREAT | O_TRUNC);
                        write(output_file, json_string_value(license), strlen(json_string_value(license)));
                        close(output_file);
                        licensed = confirmLicense();
                    }

                    if(mac && json_is_string(mac))
                    {
                        if (sscanf(json_string_value(mac), "%02x%02x%02x%02x%02x%02x", &temp_mac[0], &temp_mac[1], &temp_mac[2], &temp_mac[3], &temp_mac[4], &temp_mac[5]) != 6)
                        {
                            invalid_mac = 1;
                        }
                        printf("valid license %d, valid mac %d\n", (licensed), (invalid_mac == 0));

                        if((invalid_mac == 0) && (licensed))
                        {
                            writemac(temp_mac);
                            //perform write to DB
                        }
                    }
                }

                json_object_set_new(root, "licensed", json_boolean(licensed));
                if(licensed)
                {
                    json_object_set_new(root, "license_type", json_string((const char *)"STANDARD"));
                }
                else
                {
                    json_object_set_new(root, "license_type", json_string((const char *)"NULL"));                    
                }

                if(xt_device_mac(macstr) <= 0)
                {
                    memcpy(macstr, "NOT FOUND", sizeof("NOT FOUND"));
                }

                json_object_set_new(root, "mac", json_string((const char *)macstr));
            }
            free(buf);
        }
        else if(strcmp(param_method, "POST") == 0)
        {
            char *buf = NULL;
            uint16_t buf_len;
            xt_fcgi_get_content_len(&request, &buf, &buf_len);

            /* code */
            if (strncmp(param_ru, "/wifiscan", 9) == 0)
            {
                system("iwpriv ra0 set SiteSurvey=1");
                json_object_set_new(root, "status", json_string("OK"));
            }
            else if (strncmp(param_ru, "/zigbeepermitjoin", 17) == 0)
            {
                xt_zigbee_permit_join();
                json_object_set_new(root, "status", json_string("OK"));
            }
            else if (strncmp(param_ru, "/genfile", 8) == 0)
            {
                //TODO: perform backup file join here
                json_object_set_new(root, "status", json_string("OK"));
            }            
            else if (strncmp(param_ru, "/actrypower", 11) == 0)
            {
                unsigned char len;
                uint8_t deviceID = 1;
                ACBrand_t acbrand;
                uint8_t powerval =1;
                json_error_t jerror;
                json_t *content = json_loads(buf, 0, &jerror);

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }
                //param_ru += 11;

                if (strncmp(param_ru, "/1", 2) == 0) {
                    deviceID = 1;
                }
                else if (strncmp(param_ru, "/2", 2) == 0) {
                    deviceID = 2;
                }
                else if (strncmp(param_ru, "/3", 2) == 0) {
                    deviceID = 3;
                }

                if (content)
                {
                    if (json_is_object(content))
                    {
                        json_t *ac_brand = json_object_get(content, "ac_brand");
                        json_t *model_index = json_object_get(content, "model_index");
                        json_t *power = json_object_get(content, "power");
                        xt_getacbrand_ipc(deviceID, &acbrand);

                        if(ac_brand && json_is_string(ac_brand))
                        {
                            memcpy(acbrand.ac_brand, json_string_value(ac_brand), strlen(json_string_value(ac_brand)));
                            acbrand.ac_brand[strlen(json_string_value(ac_brand))] = 0;
                        }

                        if(model_index && json_is_integer(model_index))
                        {
                            acbrand.model_index = json_integer_value(model_index);
                        }

                        if(power && json_is_integer(power))
                        {
                            powerval = json_integer_value(power);
                        }
                        xt_actrypower_ipc(deviceID, &acbrand, powerval);
                    }
                }
                json_object_set_new(root, "status", json_string("OK"));
            }
            else if (strncmp(param_ru, "/actrymode", 10) == 0)
            {
                unsigned char len;
                uint8_t deviceID = 1;
                ACBrand_t acbrand;
                uint8_t modeval = E_AC_MODE_COOL;
                json_error_t jerror;
                json_t *content = json_loads(buf, 0, &jerror);

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }
                //param_ru += 11;

                if (strncmp(param_ru, "/1", 2) == 0) {
                    deviceID = 1;
                }
                else if (strncmp(param_ru, "/2", 2) == 0) {
                    deviceID = 2;
                }
                else if (strncmp(param_ru, "/3", 2) == 0) {
                    deviceID = 3;
                }

                if (content)
                {
                    if (json_is_object(content))
                    {
                        json_t *ac_brand = json_object_get(content, "ac_brand");
                        json_t *model_index = json_object_get(content, "model_index");
                        json_t *mode = json_object_get(content, "mode");
                        xt_getacbrand_ipc(deviceID, &acbrand);

                        if(ac_brand && json_is_string(ac_brand))
                        {
                            memcpy(acbrand.ac_brand, json_string_value(ac_brand), strlen(json_string_value(ac_brand)));
                            acbrand.ac_brand[strlen(json_string_value(ac_brand))] = 0;
                        }

                        if(model_index && json_is_integer(model_index))
                        {
                            printf("get mode:%d\n", acbrand.model_index);

                            acbrand.model_index = json_integer_value(model_index);
                        }
                        else
                        {
                            printf("fail model%d %d\n", model_index,json_is_integer(model_index));
                        }

                        if(mode && json_is_integer(mode))
                        {
                            modeval = json_integer_value(mode);
                        }
                        printf("mode:%d\n", acbrand.model_index);
                        xt_actrymode_ipc(deviceID, &acbrand, modeval);
                    }
                }
                json_object_set_new(root, "status", json_string("OK"));
            }
            else if (strncmp(param_ru, "/file", 5) == 0)
            {
                unsigned char len;                    
                int i;
                FILE * pFile;
                char filedir_buffer[100];
                char tablename[100];
                char* endptr;

                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }

                //make sure to move after the "/" symbol
                param_ru++;

                snprintf(filedir_buffer,100,  "/tmp/%s", param_ru);

                pFile = fopen (filedir_buffer,"w");
                if (pFile!=NULL)
                {
                    fwrite(buf, 1 /*sizeof(short)*/, buf_len /*20/2*/, pFile);
                    //fputs ("fopen example",pFile);
                    fclose (pFile);
                }

                printf("got str :%s\n", param_ru);

                if((endptr = strstr(param_ru, ".csv"))!= NULL)
                {
                    //printf("tempptr: %x, endptr: %x\r\n", tempptr, endptr);
                    memcpy(tablename, param_ru, endptr-param_ru);
                    tablename[endptr-param_ru] = 0;
                    printf("got table :%s\n", tablename);
                    xt_importcsv_ipc(tablename);
                }

                json_object_set_new(root, "status", json_string("OK"));

                /*
                printf("recevied file:%d\n", buf_len);
                for(i=0; i<buf_len; i++)
                {
                    printf("%02x ",buf[i]);
                }
                printf("\n");
                */
            }
            else if(strncmp(param_ru, "/irrecog", 8) == 0)
            {
                R_CONTORLCODE_t IRData;
                R_IRMODEL_t IRModel[MAX_VALID_CODE];
                uint16_t count;
                json_t *eList = json_array();
                uint8_t len;
                json_error_t jerror;
                json_t *content = json_loads(buf, 0, &jerror);
                json_t *IR_len = json_object_get(content, "IR_len");
                json_t *IR_data = json_object_get(content, "IR_data");
                json_t *element = 0;

                memset(&IRData, 0x00, sizeof(R_CONTORLCODE_t));
                if(getdirectorylength(param_ru, &len))
                {
                    param_ru += len;
                }

                if(IR_len && json_is_integer(IR_len))
                {
                    IRData.count = json_integer_value(IR_len);
                }
                //param_ru += 11;
                if(json_is_array(IR_data))
                {
                    
                    json_array_foreach(IR_data, i, element)
                    {
                        
                        if(element && json_is_integer(element))
                        {                                            
                            IRData.usec[i] = json_integer_value(element);
                        }
                    }
                    
                }

                //count = xt_getir_log(irlog, R_BUFFER_SIZE);
                count = xt_irrecog_irext_db(&IRData, IRModel, MAX_VALID_CODE);
                
                //put into json
                for(int i=0; i<count; i++)
                {
                    json_t *device = json_object();

                    json_object_set_new(device, "ac_brand", json_string((const char *)IRModel[i].ac_brand));
                    json_object_set_new(device, "model_index", json_integer(IRModel[i].model_index));

                    json_array_append_new(eList, device);
                }
                json_object_set_new(root, "IR code", eList);
            }
#ifdef FACTORY_BUILD            
            else if(strncmp(param_ru, "/hwoledtesting", 14) == 0)
            {
                //draw new menu page in HMI
                unsigned char len;
                HMIMenu_t menu;
                menu.menu_type = 5;
                menu.timeout = 10;

                xt_hmiadd_ipc(&menu);
                json_object_set_new(root, "status", json_string("OK"));

            }
            else if(strncmp(param_ru, "/hwbuttontesting", 16) == 0)
            {
                HMIButtonState_t tempbuttonstate;

                xt_hmibuttontest_op(&tempbuttonstate);
                json_object_set_new(root, "button1", json_string(tempbuttonstate.button[0]?"OK":"NG"));
                json_object_set_new(root, "button2", json_string(tempbuttonstate.button[1]?"OK":"NG"));
                json_object_set_new(root, "button3", json_string(tempbuttonstate.button[2]?"OK":"NG"));
                json_object_set_new(root, "button4", json_string(tempbuttonstate.button[3]?"OK":"NG"));                
            }         
#endif
            free(buf);
        }
                
        //xt_fcgi_set_status(root, &request, status);
        free(output);
        // Final header EOL
        if(reply_json)
        {
            FCGX_PutStr("\r\n", 2, request.out);
            char *jstr = json_dumps(root, JSON_INDENT(3));
            FCGX_PutStr(jstr, strlen(jstr), request.out);
            free(jstr);
        }        

        json_decref(root);        

        FCGX_Finish_r(&request);

        if (wifi.setstatic || wifi.setdhcp || wifi.setwifi) {
            xt_setwifi_ipc(&wifi);
            memset(&wifi, 0, sizeof(wifi));

        }

    }
    return 0;
}
