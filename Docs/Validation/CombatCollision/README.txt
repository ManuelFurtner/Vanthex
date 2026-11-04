VANTHEX - Treffer nach Bot-Tod, 4. November 2026

Ursache: Die Todespose verändert das sichtbare Skelett, nicht die separaten unsichtbaren Body-/Head-Komponenten oder die Bewegungskapsel. Diese blieben am Todesort aufrecht. LineTraceSingle traf sie zuerst; HealthComponent verwarf Schaden am bereits toten Ziel. Dahinter stehende Gegner erhielten deshalb keinen Treffer.

Korrektur: HealthComponent deaktiviert die gesamte Charakterkollision unmittelbar beim Tod, vor den Todes-Callbacks. ResetHealth aktiviert sie wieder. Der replizierte Todesstatus besitzt denselben RepNotify für Clients; Tod und Reset fordern ein zeitnahes Netzwerkupdate an. Nicht-Charakter-Ziele behalten ihre eigene Kollisionslogik. Tote Charaktere bleiben sichtbar, blockieren aber weder Schüsse noch Bewegung. Die Formen lebender Hitboxen wurden nicht geändert.

Nachweise:
- Regression vor der Korrektur fehlgeschlagen: Erster Gegner stirbt, beide nachfolgenden Gegner bleiben trotz realer FireShot-Aufrufe unbeschädigt.
- Gleicher Test nach der Korrektur bestanden: drei Gegner hintereinander, Körper-/Kopf-Traces, reale Schüsse/Schaden, Bewegung durch ehemalige Kapseln, Gesundheitsreset, Rundenreset und weiterhin blockierende feste Deckung.
- Alle 25 Vanthex-Automation-Tests erfolgreich.
- Zwei getrennte Spielprozesse: Host und Client beobachten jeweils zwei Todesfälle und zwei Wiederherstellungen; tatsächliche Traces und Kollisionsstatus stimmen in jeder beobachteten Phase.
- Allgemeiner Gameplay-Selbsttest OVERALL PASS (vollständiges Protokoll beiliegend).
- Editor und Game Win64 Development erfolgreich gebaut.

Reproduktion:
UnrealEditor-Cmd.exe Vanthex.uproject /Engine/Maps/Entry -nullrhi -nosound -unattended -ExecCmds="Automation RunTests Vanthex.Combat.SequentialKillsAndCorpseCollision" -TestExit="Automation Test Queue Empty"
Netzwerk: Host mit /Engine/Maps/Entry?listen -port=17859 -game -nullrhi -CombatCollisionNetworkTest starten, danach separaten Client mit 127.0.0.1:17859 und demselben Testschalter.

Der Schussfolgetest isoliert Kollision und Schaden mit einer streuungsfreien Diagnosewaffe und setzt deren Cooldown zwischen den Schüssen zurück. Der Netzwerktest prüft die Übertragung von Tod/Reset und echte Traces; er simuliert keinen vollständigen Netzwerkkampf. Normale Streuung, Rückstoß, Deckung und die vorhandenen vereinfachten Körper-/Kopf-Hitboxen gelten weiterhin.

Kein GitHub-Upload in diesem Reparaturlauf. Bereits vorhandene Kartenänderungen wurden erhalten.
