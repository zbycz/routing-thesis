# Bachelor thesis - elevation minimization for bicycle routing

[raw PDF](http://rawgithub.com/zbycz/routing-thesis/master/BP_Zbytovsky_Pavel_2013.pdf)



##Návod k instalaci

Na CD je celý software zkompilován pro architekturu amd64, ale je velice snadné zkompilovat jej pro libovolný systém založený na Linuxu. Kompilátor byl použitý gcc version 4.7.2 na systému Debian Wheezy. 

Upravené soubory jsou verzovány systémem GIT, jehož složka se nachází u zdrojových kódů routina, analyzéru i srtm-readeru.

1) Závislosti jsou na knihovnách libzip a libbz2 pro načítání dat. Na systémech Debian, Ubuntu apod. možno nainstalovat přes apt-get install libzip-dev libbz2-dev.

2) Kompilace se provádí v nejvyšší složce routino příkazem make, ten zároveň i zkopíruje binární programy do složky web/bin. 

3) Pro spuštění webového serveru apache2 je nutné nejprve povolit v globální konfiguraci vhost volbu AllowOverride All, následně již je dostupný router přes http://localhost/routino/web/www/routino/ pakliže fyzicky celou složku umístíme do document rootu.

4) Pro správnou funkci je ještě nutné nastavit správná práva na složku routino/web/results/, obvykle postačí chown www-data results.



Pro přípravu vlastní routovací databáze možný následující postup:
1) Stáhnout příslušné dlaždice SRTM dat: http://dds.cr.usgs.gov/srtm/version2_1/SRTM3/ a vložit je do složky routino/web/data/srtm.

2) OSM vektorová data možno stáhnout ze serveru http://download.geofabrik.de.

3) Routovací databázi vytvoříme příkazem 
routino/web/data$ ../bin/planetsplitter data.osm 2>/dev/null


## Adresářová struktura CD

readme.txt   		...   stručný popis obsahu CD
src
	instalace.txt   ...   instalační příručka		
	routino   	...   zdrojové kódy upraveného routeru
	analyzer   	...   zdrojové kódy analyzéru
		gpx   	...   uložené záznamy z GPS		
		GeoData   ...   výškový model SRTM -- CGIAR CSI				
	srtm-hgt-reader   ...   zdrojové kódy vytvořeného SRTM readeru
		srtm   	...   ukázková data SRTM		
		aster   ...   ukázková data ASTER GDEM				
	thesis   	...   zdrojová forma práce ve formátu \LaTeX{}
		grafy   ...   zdrojové soubory v programu Gnuplot		
text  			 ...   text práce
	thesis.pdf  	 ...   text práce ve formátu PDF

