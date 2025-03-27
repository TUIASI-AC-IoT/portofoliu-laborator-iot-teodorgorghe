#ifndef _STA_AP_H_
#define _STA_AP_H_

void sta_set_ssid(const char *new_ssid);
void sta_set_password(const char *pass);

void wifi_init_sta(void);

#endif