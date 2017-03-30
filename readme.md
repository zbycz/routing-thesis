# Routino


- For newer modified routino repo see: [#1](https://github.com/zbycz/routing-thesis/issues/1) - probably contribute there
- Error with latest OSM dump see: [#3](https://github.com/zbycz/routing-thesis/issues/3)
- There is not AFAIK any official git reporsitory - **get the code from author: www.routino.org**


## Bachelor thesis - elevation minimization for bicycle routing

- [Thesis - raw PDF](http://cdn.rawgit.com/zbycz/routing-thesis/master/BP_Zbytovsky_Pavel_2013.pdf)
- [Online router](http://osm.zby.cz/routino/web/www/routino/router.html?transport=bicycle;lon1=14.45988368987878;lat1=50.08622788939752;lon2=14.45239;lat2=50.09075;length=100;lat=50.08871;lon=14.45641;zoom=15) (may not work)
- [Online GPX analyzer](http://osm.zby.cz/routino/analyzer/)




### Návod k instalaci 

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


### Adresářová struktura CD

```
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

```
