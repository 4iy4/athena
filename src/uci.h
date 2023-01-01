#ifndef UCI_H
#define UCI_H

bool uci_interpret(const char *str);
void uci_send(const char *fmt, ...);
char *uci_receive(void);

#endif
