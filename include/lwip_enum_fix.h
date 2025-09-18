#pragma once
// Must be included AFTER <ESP8266WiFi.h> and BEFORE <ESPAsyncWebServer.h>
// Undefine wl_definitions.h TCP state enum members to avoid clash with lwIP tcpbase.h

#ifdef CLOSED
  #undef CLOSED
#endif
#ifdef LISTEN
  #undef LISTEN
#endif
#ifdef SYN_SENT
  #undef SYN_SENT
#endif
#ifdef SYN_RCVD
  #undef SYN_RCVD
#endif
#ifdef ESTABLISHED
  #undef ESTABLISHED
#endif
#ifdef FIN_WAIT_1
  #undef FIN_WAIT_1
#endif
#ifdef FIN_WAIT_2
  #undef FIN_WAIT_2
#endif
#ifdef CLOSE_WAIT
  #undef CLOSE_WAIT
#endif
#ifdef CLOSING
  #undef CLOSING
#endif
#ifdef LAST_ACK
  #undef LAST_ACK
#endif
#ifdef TIME_WAIT
  #undef TIME_WAIT
#endif
