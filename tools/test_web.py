"""Tests für die öffentliche Seite web/ (W-02, W-03): Verweise auf Bilder und Anker stimmen, Inhalte nach Vorgabe."""
import re
import unittest
from pathlib import Path

WEB = Path(__file__).resolve().parent.parent / "web"
SEITEN = ("index.html", "en/index.html")  # W-09: keine Sponsoring-Seite mehr; W-11: EN als getrennte Seite (ADR-046)
ASSET = r"((?:\.\./)*assets/[^\"#]+)"


def lade(name):
    return re.sub(r"<!--.*?-->", "", (WEB / name).read_text(encoding="utf-8"), flags=re.S)


HTML = {n: lade(n) for n in SEITEN}


class WebTest(unittest.TestCase):
    def test_lokale_dateien_existieren(self):
        for name, html in HTML.items():
            refs = set(re.findall(r'(?:src|href)="' + ASSET + '"', html))
            refs |= set(re.findall(r'url\("((?:\.\./)*assets/[^"]+)"\)', html))
            self.assertTrue(refs, name)
            ordner = (WEB / name).parent
            fehlt = sorted(r for r in refs if not (ordner / r).resolve().is_file())
            self.assertEqual(fehlt, [], name)

    def test_css_verweise_existieren(self):
        css = (WEB / "assets" / "style.css").read_text(encoding="utf-8")
        refs = set(re.findall(r'url\("(assets/[^"]+)"\)', css))
        self.assertEqual(sorted(r for r in refs if not (WEB / r).is_file()), [])

    def test_anker_haben_ziel(self):
        ids = {n: set(re.findall(r'\bid="([^"]+)"', h)) for n, h in HTML.items()}
        for name, html in HTML.items():
            for seite, anker in re.findall(r'href="([a-z]+\.html)?#([^"]+)"', html):
                ziel = (str(Path(name).parent / seite) if seite else name).replace("./", "")
                self.assertIn(ziel, ids, f"{name}: {seite}")
                self.assertIn(anker, ids[ziel], f"{name}: {seite}#{anker}")

    def test_seitenlinks_zeigen_auf_vorhandene_seiten(self):
        for name, html in HTML.items():
            for seite in re.findall(r'href="([a-z]+\.html)', html):
                self.assertTrue((WEB / Path(name).parent / seite).is_file(), f"{name}: {seite}")

    def test_bilder_haben_alt_attribut(self):
        for name, html in HTML.items():
            for tag in re.findall(r"<img\b[^>]*>", html):
                self.assertIn("alt=", tag, f"{name}: {tag}")

    def test_nur_ein_head_und_body(self):
        for name, html in HTML.items():
            for t in ("<head>", "<body>", "</html>"):
                self.assertEqual(html.count(t), 1, f"{name}: {t}")

    def test_keine_platzhalter_links_zu_fremden_repos(self):
        for html in HTML.values():
            self.assertNotIn("claude:viewport", html)

    def test_seiten_haben_keine_werkzeugnamen(self):
        for name, h in HTML.items():
            for wort in ("Fusion 360", "Siemens NX", "CNC", "C++"):
                self.assertNotIn(wort, h, name)

    def test_keine_sponsoring_seite_und_keine_oeffentlichen_stufen(self):
        # W-09 (Mensch, 2026-11-11): keine sponsoring.html, Pakete/Stufen/Preise/Budget nicht öffentlich
        self.assertFalse((WEB / "sponsoring.html").exists())
        for name, h in HTML.items():
            self.assertNotIn("sponsoring.html", h)
            # W-14 (Mensch, 2026-11-12): "Hauptpartner"/"Supporter" sind als Bezeichnung erlaubt, Pakete/Preise bleiben verboten
            for wort in ("Sponsoring-Pakete", "Budget", "10'000", "ab 500", "ab 1'500",
                         "tierlbl", "slot-main", "grid3", "grid6", "Kilobyte", "Megabyte", "Gigabyte", "35'000", "€"):
                self.assertNotIn(wort, h, f"{name}: {wort}")

    def test_ids_sind_eindeutig(self):
        for name, html in HTML.items():
            ids = re.findall(r'\bid="([^"]+)"', html)
            self.assertEqual(sorted(i for i in set(ids) if ids.count(i) > 1), [], name)

    def test_keine_webfonts_nur_systemschrift(self):
        css = (WEB / "assets" / "style.css").read_text(encoding="utf-8")
        for name, html in HTML.items():
            self.assertNotIn("fonts.googleapis", html, name)
            self.assertNotIn("fonts.gstatic", html, name)
        for schrift in ("Barlow", "Chakra", "JetBrains"):
            self.assertNotIn(schrift, css)
            self.assertNotIn(schrift, HTML["index.html"])
        self.assertIn("system-ui", css)

    def test_kein_youtube(self):
        for name, html in HTML.items():
            self.assertNotIn("youtube", html.lower(), name)

    def test_kontakt_ist_die_teamadresse_ohne_personenname(self):
        for name, html in HTML.items():
            self.assertNotIn("lechnerm", html, name)
            self.assertIn("frostbyte.eth.robotics@gmail.com", html, name)
            self.assertNotIn("Lechner · Frostbyte", html, name)
        self.assertNotIn("lechnerm", (WEB / "assets" / "site.js").read_text(encoding="utf-8"))
        self.assertIn("frostbyte.eth.robotics@gmail.com", (WEB / "assets" / "site.js").read_text(encoding="utf-8"))

    def test_livestream_zahl_und_fusszeile(self):
        self.assertIn("160'000", HTML["index.html"])
        for html in HTML.values():
            self.assertNotIn("Ziel: Top 16", html)
            self.assertNotIn("Top 16", html)
            self.assertNotIn("top 16", html.lower())

    def test_social_links_sind_echt(self):
        for name, h in HTML.items():
            self.assertIn('href="https://www.instagram.com/frostbyte.robotics/"', h, name)
            self.assertIn('href="https://www.linkedin.com/company/frostbyterobotics"', h, name)
            self.assertNotIn('href="#"', h, name)

    def test_freie_fotos_haben_bildnachweis(self):
        h = HTML["index.html"]
        for datei in ("worlds-match.jpg", "team-trophy.jpg"):
            self.assertIn(f"assets/{datei}", h)
        self.assertIn("Rick Tyler", h)
        self.assertIn("CC BY-SA 3.0", h)
        self.assertIn("Wrc Ynapmoc", h)
        self.assertIn("CC BY 3.0", h)

    def test_mosaik_fotos_haben_bildnachweis_und_alte_dateien_sind_weg(self):
        h = HTML["index.html"]
        for i in (1, 2, 4):
            self.assertIn(f"assets/liga-{i}.jpg", h)
            self.assertTrue((WEB / "assets" / f"liga-{i}.jpg").is_file())
        self.assertIn("assets/arena.jpg", h)  # Entscheidung des Menschen: Arena-Foto aus der eigenen Vorlage, siehe ADR-036
        for titel in ("One last adjustment", "VEX Starstruck Standard Fields", "Judgment day at Sunset Station"):
            self.assertIn(titel, h)
        self.assertIn("Sean Spicer", h)
        self.assertIn("CC BY-SA 4.0", h)
        self.assertIn("Lisa Litchfield", h)
        # Herkunft und Rechte undokumentiert: darf nicht zurückkommen
        for alt in ("event-field", "vex-stage", "champions.jpg"):
            self.assertNotIn(alt, h)
            self.assertFalse((WEB / "assets" / alt).exists() if alt.endswith(".jpg") else False)

    def test_skript_prueft_fehlende_elemente(self):
        # Beide Seiten teilen assets/site.js; die Sponsoringseite hat kein Hero (#snow) und keine Anker auf Fremdseiten.
        js = (WEB / "assets" / "site.js").read_text(encoding="utf-8")
        self.assertIn("if(burger&&menu)", js)
        self.assertIn("if(copyBtn)", js)
        self.assertIn('a[href^="#"]', js)

    def test_seiten_verweisen_auf_news_in_navigation_und_fusszeile(self):
        for name in SEITEN:
            html = HTML[name]
            nav, fuss = html.split("</nav>")[0], html.split("<footer")[1]
            ziel = "../news/index.html" if name.startswith("en/") else "news/index.html"
            self.assertIn(f'href="{ziel}"', nav, name)
            self.assertIn(f'href="{ziel}"', fuss, name)

    def test_techlabs_schreibweise(self):
        for name, h in HTML.items():
            self.assertIn("TechLabs", h, name)
            for falsch in ("TECHLABS", "Techlabs", "Tech Labs"):
                self.assertNotIn(falsch, h, name)

    def test_hauptsponsor_novagear_schreibweise(self):
        for name, h in HTML.items():
            self.assertIn("Novagear", h, name)
            self.assertNotIn("NOVAGEAR", h, name)

    def test_nur_bestaetigte_partner(self):
        # W-11: Folie 12 des Decks nennt weitere Namen, die noch nicht bestätigt sind
        for name, h in HTML.items():
            # W-14: TechLabs ist vom Menschen bestätigt (2026-11-12)
            for fremd in ("Bachofen", "rosttec", "Student Project House"):
                self.assertNotIn(fremd, h, f"{name}: {fremd}")

    def test_kein_scroll_fortschrittsbalken(self):
        # W-09: blauer Balken im Nav, der beim Scrollen von links nach rechts wächst
        for name in SEITEN:
            self.assertNotIn("progress", HTML[name], name)
            self.assertNotIn("youtube", HTML[name].lower(), name)
        self.assertNotIn(".progress", (WEB / "assets" / "style.css").read_text(encoding="utf-8"))
        self.assertNotIn("marquee", HTML["index.html"])  # blaues Laufband unter dem Kopf
        self.assertNotIn("progress", (WEB / "assets" / "site.js").read_text(encoding="utf-8"))

    def test_keine_einzelportraits_dafuer_teamfoto_platz(self):
        h = HTML["index.html"]
        for datei in ("mate", "david", "arved", "prof"):
            self.assertNotIn(f"assets/{datei}.jpg", h)
            self.assertFalse((WEB / "assets" / f"{datei}.jpg").exists(), datei)
        self.assertIn('id="teamfoto"', h)  # freier Platz, kein erfundenes Bild
        for name in ("Máté Lechner", "David Čop", "Arved Janssen", "Stefan Leutenegger"):
            self.assertIn(name, h)

    def test_partnerwand_gross_auf_hauptseite_vor_dem_ziel(self):
        for name, h in HTML.items():
            self.assertIn('id="partner"', h, name)
            self.assertLess(h.index('id="partner"'), h.index('id="ziel"'), name)
            self.assertIn('href="#partner"', h.split("</nav>")[0], name)

    # ---- W-14: Partner mit Logos (Stufen) und zwei Formulare ----
    def test_partnerstufen_mit_logos(self):
        # Mensch 2026-11-12: Novagear Hauptpartner, TechLabs Supporter; norelem vorerst weglassen
        for name, h in HTML.items():
            wand = h[h.index('id="partner"'):h.index('id="ziel"')]
            self.assertNotIn("norelem", h, name)
            haupt = re.search(r'class="ptier ptier-main[^"]*".*?(?=class="ptier ptier-sup|$)', wand, re.S).group(0)
            sup = wand[wand.index('class="ptier ptier-sup'):]
            self.assertRegex(haupt, r'<img src="(?:\.\./)?assets/sponsoren/novagear\.png" alt="Novagear" width="\d+" height="\d+"')
            self.assertRegex(sup, r'<img src="(?:\.\./)?assets/sponsoren/techlabs\.png" alt="TechLabs" width="\d+" height="\d+"')
            self.assertNotIn("techlabs.png", haupt, name)
            self.assertNotIn("novagear.png", sup, name)
            self.assertIn("Hauptpartner" if name == "index.html" else "Main partner", haupt, name)
            self.assertIn("Supporter", sup, name)
            self.assertIn('href="#partner-werden"', wand, name)
            self.assertNotIn("Ihr Logo", wand, name)
            self.assertNotIn("Your logo", wand, name)

    def test_formulare_team_und_partner(self):
        felder = {
            "form-team": ("t-name", "t-mail", "t-studium", "t-gebiet", "t-text"),
            "form-partner": ("p-firma", "p-person", "p-mail", "p-art", "p-text"),
        }
        for name, h in HTML.items():
            for form_id, ids in felder.items():
                m = re.search(rf'<form\b[^>]*\bid="{form_id}"[^>]*>(.*?)</form>', h, re.S)
                self.assertIsNotNone(m, f"{name}: {form_id}")
                kopf = re.search(rf'<form\b[^>]*\bid="{form_id}"[^>]*>', h).group(0)
                self.assertIn('data-to="frostbyte.eth.robotics@gmail.com"', kopf, name)
                self.assertIn("data-subject=", kopf, name)
                self.assertIn("data-sent=", kopf, name)
                self.assertNotIn("action=", kopf, name)  # nie an einen Server senden
                for i in ids:
                    self.assertRegex(m.group(1), rf'<(?:input|select|textarea)\b[^>]*\bid="{i}"', f"{name}: {i}")
                    self.assertRegex(m.group(1), rf'<label\b[^>]*\bfor="{i}"', f"{name}: label {i}")
                for mail in re.findall(r'<input\b[^>]*\bid="[tp]-mail"[^>]*>', m.group(1)):
                    self.assertIn('type="email"', mail)
                    self.assertIn("required", mail)
                self.assertIn("<noscript>", h[h.index(f'id="{form_id}"') - 600:h.index(f'id="{form_id}"') + 4000], name)
            self.assertLess(h.index('id="bewerben"'), h.index('id="partner-werden"'), name)
            self.assertLess(h.index('id="partner-werden"'), h.index('id="kontakt"'), name)
            self.assertIn('href="#bewerben"', h.split("</nav>")[0], name)

    def test_formular_skript_ohne_server_und_ohne_location(self):
        js = (WEB / "assets" / "site.js").read_text(encoding="utf-8")
        self.assertIn("mailto:", js)
        self.assertIn("encodeURIComponent", js)
        self.assertIn("checkValidity", js)
        for verboten in ("location", "fetch(", "XMLHttpRequest", "FormData", "sendBeacon"):
            self.assertNotIn(verboten, js)
        # Beschriftungen kommen aus der Seite, nicht aus dem Skript (ADR-046): kein deutscher Mail-Text im Skript
        for text in ("Bewerbung", "Betreff", "Nachricht", "Name:"):
            self.assertNotIn(text, js)

    # ---- W-11: Zweisprachigkeit (ADR-046) ----
    def test_sprache_und_umschalter(self):
        de, en = HTML["index.html"], HTML["en/index.html"]
        self.assertIn('<html lang="de">', de)
        self.assertIn('<html lang="en">', en)
        for name, h in HTML.items():
            for lang in ("de", "en", "x-default"):
                self.assertIn(f'hreflang="{lang}"', h, name)
            self.assertIn('rel="canonical"', h, name)
            nav, fuss = h.split("</nav>")[0], h.split("<footer")[1]
            self.assertIn('class="lang"', nav, name)
            self.assertIn('class="lang"', fuss, name)
        self.assertIn('hreflang="en" href="en/index.html"', de.split("</nav>")[0])
        self.assertIn('hreflang="de" href="../index.html"', en.split("</nav>")[0])

    def test_keine_automatische_weiterleitung_nach_sprache(self):
        js = (WEB / "assets" / "site.js").read_text(encoding="utf-8")
        for verboten in ("navigator.language", "location.replace", "location.href", "location.assign"):
            self.assertNotIn(verboten, js)
        for h in HTML.values():
            self.assertNotIn('http-equiv="refresh"', h)

    def test_en_und_de_haben_gleiche_struktur(self):
        # Schutz gegen Auseinanderlaufen: gleiche ids, gleiche Abschnittsreihenfolge, gleiche Bilder
        de, en = HTML["index.html"], HTML["en/index.html"]
        self.assertEqual(sorted(re.findall(r'\bid="([^"]+)"', de)), sorted(re.findall(r'\bid="([^"]+)"', en)))
        sek = lambda h: re.findall(r'<(?:section|header)\b[^>]*\bid="([^"]+)"', h)
        self.assertEqual(sek(de), sek(en))
        bild = lambda h: sorted(re.findall(r'(?:src|href)="(?:\.\./)*(assets/[^"#]+)"', h))
        self.assertEqual(bild(de), bild(en))
        self.assertEqual(len(re.findall(r"<img\b", de)), len(re.findall(r"<img\b", en)))
        zahl = lambda h: sorted(re.findall(r'data-count="(\d+)"', h))
        self.assertEqual(zahl(de), zahl(en))

    def test_en_ist_wirklich_englisch(self):
        en = HTML["en/index.html"]
        text = re.sub(r"<[^>]+>", " ", en)
        for deutsch in ("Unsere", "Wir ", "Zürich ins", "Kontakt", "Saison", "Partner gewinnen", "Spielfeld"):
            self.assertNotIn(deutsch, text, deutsch)
        self.assertIn("Zurich", text)
        self.assertNotIn("Zürich", text)

    def test_en_news_bleibt_deutsch_und_ist_markiert(self):
        en = HTML["en/index.html"]
        self.assertIn('href="../news/index.html" hreflang="de"', en.split("</nav>")[0])

    # ---- W-11: Fakten aus dem Pitch-Deck ----
    def test_guinness_block_mit_quelle(self):
        for name, h in HTML.items():
            self.assertIn('id="weltrekord"', h, name)
            self.assertIn("Guinness", h, name)
            for z in ("20000", "1700", "50", "2400"):
                self.assertIn(f'data-count="{z}"', h, name)
            self.assertNotIn("60 L", h, name)
            self.assertNotIn("60 countries", h, name)
            self.assertIn("RECF", h, name)  # Quellenzeile
            self.assertIn("data-sep=", h, name)

    def test_zaehler_hat_tausendertrenner_und_endwert_im_html(self):
        # ohne JavaScript und bei reduzierter Bewegung muss der Endwert schon im HTML stehen
        for name, h in HTML.items():
            for z, text in (("20000", "20'000+"), ("1700", "1'700+"), ("2400", "2'400+")):
                self.assertRegex(h, rf'data-count="{z}"[^>]*>{re.escape(text)}<', name)

    # ---- W-11: Maßstabsbild der Roboter ----
    def test_massstab_zeigt_beide_roboter_auf_dem_feld(self):
        for name, h in HTML.items():
            self.assertIn('id="massstab"', h, name)
            self.assertIn('viewBox="0 0 144 144"', h, name)  # 12 ft = 144 Zoll, 1 Einheit = 1 Zoll
            self.assertRegex(h, r'width="24" height="24"', name)
            self.assertRegex(h, r'width="15" height="15"', name)

    def test_css_klammern_ausgeglichen(self):
        # W-11: ein offenes @media hatte alle folgenden Regeln verschluckt (Guinness-Block, Sprunglink unformatiert)
        css = re.sub(r"/\*.*?\*/", "", (WEB / "assets" / "style.css").read_text(encoding="utf-8"), flags=re.S)
        self.assertEqual(css.count("{"), css.count("}"))
        tief = 0
        for zeile in css.split("\n"):
            if zeile.lstrip().startswith("@media") and tief > 0:
                self.fail(f"verschachteltes @media (offene Klammer davor): {zeile[:60]}")
            tief += zeile.count("{") - zeile.count("}")

    def test_partnerwand_css(self):
        # W-11: die Regel für die große Wand war verstümmelt; W-14: Stufen-Markup .partners/.ptier statt .wall-big
        css = (WEB / "assets" / "style.css").read_text(encoding="utf-8")
        self.assertRegex(css, r"\.partners\{[^}]*display:grid")
        self.assertRegex(css, r"\.plogo img\{[^}]*width:100%")
        self.assertNotIn(".wall-big .wall-big", css)

    def test_skript_ohne_fremdquellen(self):
        js = (WEB / "assets" / "site.js").read_text(encoding="utf-8")
        for verboten in ("http://", "https://", "fetch(", "XMLHttpRequest", "import("):
            self.assertNotIn(verboten, js)
        for name, h in HTML.items():
            for src in re.findall(r'<script[^>]*\bsrc="([^"]+)"', h):
                self.assertTrue(src.startswith(("assets/", "../assets/")), f"{name}: {src}")

    def test_sprunglink_und_reduzierte_bewegung(self):
        for name, h in HTML.items():
            self.assertIn('class="skip"', h, name)
        css = (WEB / "assets" / "style.css").read_text(encoding="utf-8")
        self.assertIn("prefers-reduced-motion", css)


if __name__ == "__main__":
    unittest.main()
