# ADR-040: Funk-Link-Test (Ping/Pong, Latenz und Verlust)

Status: angenommen (L-01, 2026-11-06)

## Entscheidung
- Logik in `lib/util/link_test.hpp` ohne PROS und ohne dynamischen Speicher: `LinkPacket` (14 Byte: Art, Nummer, `t_ping_ms`, `t_pong_ms`, Prüfsumme), `LinkPinger`, `LinkReflector`. Senden und Empfangen in `src/link_test.hpp` (`pros::Link`, Paketmodus), eigene Schleife mit `delay()`, aktiv nur mit `config::link_test_enabled`; nie zusammen mit dem Steuer-Task und nie im Match (`static_assert` gegen `port_check_enabled`).
- Gemessen wird nur auf dem Pinger mit seiner eigenen Uhr (Differenz `pros::millis()`, überlaufsicher). Die Uhren der Roboter sind nicht synchron; `t_pong_ms` steht nur zur Diagnose im Paket. Gemessen wird die Umlaufzeit (RTT); Einweglatenz ≈ RTT/2 ist eine Annahme und wird nicht ausgewiesen.
- Mehrere Pings gleichzeitig unterwegs (32 Plätze, Platz = Nummer mod 32). Je Ping genau ein Eintrag, sobald der Ausgang feststeht: Pong (RTT) oder Timeout (verloren). Spätere Pongs zählen getrennt: `late` (nach Timeout), `duplicate` (zweimal), `reordered` (nach dem Pong eines neueren Pings, zählt auch als `received`), `unmatched` (unbekannt/zu alt/Sendezeit passt nicht). Läuft der Platz über, gilt der älteste offene Ping als verloren.
- Protokoll `frostbyte-linklog v1` (eigene kleine Datei `/usd/linklog_<roboter>.csv`, nicht das Roboter-Log aus `docs/log_format.md`): Kopfzeile `# frostbyte-linklog v1 robot=<..> role=<ping|echo> build=<..>`, Spalten `seq,t_send_s,rtt_ms`, leere `rtt_ms`-Zelle = verloren. Änderungen nur mit neuer Versionsnummer.
- `tools/link_report.py` wertet es aus: gesendet/beantwortet/verloren, Verlustrate, längste Verlustserie, RTT min/Median/P95/max. Mit `--max-loss` / `--max-p95-ms` Exit 1 bei Überschreitung oder wenn nichts gemessen wurde.

## Messgrößen und Grenzwerte (TODO(HW), L-04)
Verlustrate, längste Verlustserie, RTT-Median, RTT-P95, RTT-Max. Die Grenzwerte stehen bewusst nirgends im Code: sie werden erst mit der Messung am Roboter (L-04) aus echten Läufen festgelegt, danach hier eintragen. Zu messen: mit und ohne Abstand/Abschirmung, mit laufenden Motoren, mit zweitem Funkgerät am Feld.

## Annahmen (TODO(HW), L-04)
- Funk-Port, Link-Id und Sender-/Empfängerrolle sind nicht festgelegt (`config/`, `link_port = 0` stoppt das Programm mit Hinweis).
- Verhalten von `receive()` ohne Daten, Brauchbarkeit von `raw_receivable_size()` im Paketmodus und ob nach `EBADMSG` der Puffer geleert werden muss, sind ungeprüft.
- Sendefehler von `transmit()` werden nicht ausgewertet; sie erscheinen als Verlust.
- Startwerte `link_period_ms = 50`, `link_timeout_ms = 500` sind Software-Entscheidungen.
