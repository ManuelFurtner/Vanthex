VANTHEX – animierte Trefferzonen, Core-Randfall und Zweihand-Waffenhaltung
04.11.2026, Unreal Engine 5.8.3, Projekt H:\Vanthex

UMGESETZT
1. ARC verwendet die 19 Kollisionskörper seines bestehenden Physics Assets.
   Kopf, Hals, Rumpf, Arme, Hände, Beine und Füße folgen dem ausgewerteten Skelett.
   Bewegungskapsel und alte Body-/Head-Würfel fangen keine Sicht-/Schusstraces ab,
   solange das Skelett-Physics-Asset geladen ist. Die alten Würfel bleiben nur
   Rückfalllösung bei fehlenden Charakter-Assets. Kopftreffer nutzen Hit.BoneName.
   Tote Charaktere blockieren keine Schüsse. Wiederbelebung wertet die lebende
   Pose synchron aus, bevor ihre Trefferkörper wieder aktiviert werden.
   Bots zielen auf den tatsächlichen animierten Oberkörper.

2. Ein in einer Extraction-Zone abgelegter Core merkt sich Zone und Besitzrevision.
   Nimmt das eigene Team ihn dort wieder auf, bleiben Extraktionsfortschritt sowie
   Eliminierungs-/Timeout-Aufschub erhalten. Verlassen der Zone, gegnerischer
   Besitzwechsel und Rundenreset heben den Zustand auf. Ein nur getragener Core
   ohne vorherige Ablage erhält keine zusätzliche Eliminierungs-Ausnahme.
   TryPickup erfasst auch Ablagen zwischen zwei 0,1-Sekunden-Clock-Ticks.

3. Separate, nur für den Besitzer sichtbare Ego-Arme greifen an die tatsächliche
   Waffenposition. Beide Hände folgen ADS, Rückstoß, Wechsel und Nachladebewegung;
   beim Messer bleibt die zweite Hand in einer freien Schutzhaltung.
   Waffenspezifische Griffpunkte, Fingerstellung, Unterarmdrehung und Weltmaßstab
   ersetzen die bisherige schwebende Haltung. Körper und Beine bleiben separat.
   SK_ARC_FirstPersonArms ist aus dem bereits versionierten Mannequin abgeleitet:
   unverändertes Skelett/Bind Pose/Skinning, nur armgewichtete Dreiecke, kein Rumpf.
   Das Originalmesh bleibt unverändert. Das neue Mesh besitzt keine Spielkollision.

PRÜFUNG
Erfolgreiche Editor- und Game-Development-Builds (Builds.txt).
- PoseHitTest: 132/132 mit lokalen Modellen; 132/132 mit öffentlichen Eigenmodellen.
- ExtractionRules: 136/136; OperatorValidation: 39/39; VanthexSelfTest: 64/64.
- GadgetIntegration: 58/58; WeaponProfiles: 24/24; AimTest: 15/15.
- Unreal Automation: 26 erfolgreiche Tests, 0 fehlgeschlagen, 0 Testwarnungen.
- Vier Netzwerkszenarien jeweils auf Host und Client PASS: freundliche Core-
  Wiederaufnahme, alle sechs Spieler tot, zwei Tod-/Revive-Zyklen, Client-ADS-RPCs.
- Gerenderte Ansichten: 12 Waffenansichten ohne Fab-Bundle, 9 Visieransichten mit
  lokalem Bundle, 4 Körperansichten. Ausgewählte Bilder befinden sich in Images.

Die vollständigen Ergebnisdateien liegen neben diesem Bericht. Die gezielten
Pose-Prüfungen messen echte Bone-Positionen und Physics-Traces in GridV4, nicht
nur konfigurierte Zahlen. Geprüft werden Stehen, Ducken, Bewegung, vertikales
Zielen, Kopf-/Körperschaden, Tod und sofortige Wiederbelebung sowie beide Hände
an neun Schusswaffen in Hüft-/ADS-Haltung und beim M4-Nachladen.

Die Core-Prüfung deckt beide Teams und beide Zonen ab, einschließlich abgelaufener
Rundenzeit, freundlicher Wiederaufnahme, Gegneraufnahme und vollständiger
Eliminierung. Zusätzliche getrennte Host-/Client-Prozesse prüfen Replikation.

Während der Arbeit gefundene Regressionen wurden behoben und erneut geprüft:
- Ein alter Nahkampftest zielte auf einen leeren Teil der früheren großen Box.
  Er zielt jetzt auf den echten Torso; Schaden, Wandblockade, Reichweite und
  Angriff beim Tod werden weiterhin mit echten Messerangriffen geprüft.
- Der Netzwerk-Kollisionstest prüfte die Mitte der Bewegungskapsel. Nach der
  Bot-Übernahme kann diese außerhalb der animierten Trefferkörper liegen.
  Er prüft jetzt den tatsächlichen Beckenkörper und verlangt einen Skeletal-Hit.
- Ein eigener Test wartet die Todesanimation ab und prüft anschließend die
  aufrechte Pose und Treffer unmittelbar im selben Frame der Wiederbelebung.
- Sichtbare Rumpfreste der zunächst vollständigen Ego-Kopie wurden durch das
  dedizierte Armmesh beseitigt; nicht lediglich im Test ausgeblendet.

REPRODUKTION
UnrealEditor-Cmd.exe "H:\Vanthex\Vanthex.uproject" /Game/Vanthex/Maps/GridV4
  -game -nullrhi -nosound -unattended -PoseHitTest
Zusätzlich -VanthexOwnWeaponModels prüft die öffentlichen Waffen ohne Fab-Bundle.
Weitere einzelne Testflags: -ExtractionRulesTest, -OperatorTest, -AimTest,
  -GadgetTest, -WeaponProfileTest; -VanthexSelfTest verwendet /Engine/Maps/Entry.
Automation: -ExecCmds="Automation RunTests Vanthex"
  -TestExit="Automation Test Queue Empty"
Netzwerk: gleiche Testflags auf Listen-Host (GridV4?listen) und beitretendem Client:
  -LooseCoreNetworkTest -EliminationNetworkTest -FriendlyPickupNetworkTest
  -LooseCoreNetworkTest -EliminationNetworkTest
  -CombatCollisionNetworkTest
  -AimNetworkTest
Armmesh reproduzieren: -run=VanthexBuildArt -BuildOperatorArms
Renderbilder: -WeaponModelShots, -AimShots oder -BodyViewShots statt NullRHI,
  mit -game -RenderOffscreen -ResX=1280 -ResY=720.

GRENZEN
Der Editor-Automation-Start protokolliert vor Engine-Initialisierung 17 generische
"Condition failed"-Zeilen. Diese sind nicht Teil der anschließend ausgeführten
26 Vanthex-Testfälle; der exportierte Testbericht meldet 26 Success/0 Fail.
Deshalb wird kein vollständig meldungsfreies Editor-Startprotokoll behauptet.
Physics-Asset-Körper sind animierte Näherungen der Figur, keine pixelgenauen
Dreieckstreffer. Die Handhaltung ist prozedurales IK mit Fingerposen; sie ersetzt
keine individuell animierten Motion-Capture-Sequenzen für jede Waffe.
Netzwerktests liefen als getrennte lokale Listen-Host-/Client-Prozesse; keine
neue Dedicated-Server-, WAN-/Latenz-, Cook- oder Paketfreigabe wird behauptet.
Lokale Fab-Originale und abgeleitete Fab-Assets bleiben vom öffentlichen Upload
ausgeschlossen. Das neue Armmesh basiert auf dem bereits enthaltenen Mannequin.
Die lokale Engine-GUID und vom Benutzer erzeugte Solution-Dateien bleiben lokal.
