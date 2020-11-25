/*********************************************************************
	Symulacja obiekt�w fizycznych ruchomych np. samochody, statki, roboty, itd.
	+ obs�uga obiekt�w statycznych np. teren.
	**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>
#include <gl\gl.h>
#include <gl\glu.h>
#include "obiekty.h"
#include "grafika.h"

//#include "wektor.h"
extern FILE *f;
extern Teren teren;
extern bool czy_rysowac_ID;


ObiektRuchomy::ObiektRuchomy()             // konstruktor                   
{



}

ObiektRuchomy::ObiektRuchomy(Teren *teren)
{
	ObiektRuchomy();
	//iID = (unsigned int)(clock() % 1000);  // identyfikator obiektu
	iID = (unsigned int)(rand() % 1000);  // identyfikator obiektu
	fprintf(f, "MojObiekt->iID = %d\n", iID);


	F = Fb = 0;	// si�y dzia�aj�ce na obiekt 
	F_max = 20000;
	kat_skretu_kol_max = PI*60.0 / 180;
	ham = 0;			// stopie� hamowania
	ilosc_paliwa = 3.0;
	predkosc_krecenia_kierownica = 0;  // pr�dko�� kr�cenia kierownic� w rad/s
	czy_kierownica_trzymana = 0;  // informacja czy kieronica jest trzymana


	//Fy = m*9.81;        // si�a nacisku na podstaw� obiektu (na ko�a pojazdu)
	masa_wlasna = 1000.0;			// masa obiektu [kg]
	dlugosc = 6.0;
	szerokosc = 2.3;
	wysokosc = 1.7;
	przeswit = 0.05;     // wysoko�� na kt�rej znajduje si� podstawa obiektu
	dl_przod = 1.0;     // odleg�o�� od przedniej osi do przedniego zderzaka 
	dl_tyl = 0.2;       // odleg�o�� od tylniej osi do tylniego zderzaka
	promien = sqrt(dlugosc*dlugosc + szerokosc*szerokosc + wysokosc*wysokosc) / 2 / 1.15;
	predkosc_powrotu_kierownicy = 0.5; // pr�dko�� powrotu kierownicy w rad/s (gdy zostateie puszczona)
	

	//wPol.y = przeswit+wysokosc/2 + 20;
	//wPol.x = teren->srodek.x;
	//wPol.z = teren->srodek.z;
	this->teren = teren;
	stan.wPol = teren->PunktMax(Wektor3(0, 1, 0)) + Wektor3(0, 1, 0)*(przeswit + wysokosc / 2 + 20);

	//fprintf(f, "Pol poczatkowe mojego obiektu = (%f, %f, %f)\n", wPol.x, wPol.y, wPol.z);
	//fprintf(f, "Srodek kuli = (%f, %f, %f)\n", teren->srodek.x, teren->srodek.y, teren->srodek.z);

	stan.wV_kat = Wektor3(0, 1, 0) * 0;//40;  // pocz�tkowa pr�dko�� k�towa (w celach testowych)
	

	// obr�t obiektu o k�t 30 stopni wzgl�dem osi y:
	kwaternion qObr = AsixToQuat(Wektor3(0, 1, 0), 0.1*PI / 180.0);
	stan.qOrient = qObr*stan.qOrient;
	stan.kat_skretu_kol = 0;


	iID_kolid = -1;           // na razie brak kolizji 

}

ObiektRuchomy::~ObiektRuchomy()            // destruktor
{
}

void ObiektRuchomy::ZmienStan(StanObiektu __stan)  // przepisanie podanego stanu 
{                                                // w przypadku obiekt�w, kt�re nie s� symulowane
	this->stan = __stan;
}

StanObiektu ObiektRuchomy::Stan()                // metoda zwracaj�ca stan obiektu ��cznie z iID
{
	return stan;
}



void ObiektRuchomy::Symulacja(float dt)          // obliczenie nowego stanu na podstawie dotychczasowego,
{                                                // dzia�aj�cych si� i czasu, jaki up�yn�� od ostatniej symulacji

	if (dt == 0) return;

	float tarcie = 1.8;            // wsp�czynnik tarcia obiektu o pod�o�e 
	float tarcie_obr = tarcie;     // tarcie obrotowe (w szczeg�lnych przypadkach mo�e by� inne ni� liniowe)
	float tarcie_toczne = 0.05;    // wsp�czynnik tarcia tocznego
	float sprezystosc = 0.1;       // wsp�czynnik spr�ysto�ci (0-brak spr�ysto�ci, 1-doskona�a spr�ysto��) 
	float g = 9.81;                // przyspieszenie grawitacyjne na powierzchni planety, p�niej maleje
	// wraz z kwadratem odleglo�ci
	// zmniejszenie g z uwagi na odleglo�� od �rodka:
	float odleglosc_od_srodka = (teren->srodek - stan.wPol).dlugosc();
	if (odleglosc_od_srodka > teren->promien_sredni)
		g *= teren->promien_sredni*teren->promien_sredni / odleglosc_od_srodka / odleglosc_od_srodka;

	stan.masa = masa_wlasna + ilosc_paliwa;  // masa ca�kowita
	float Fy = stan.masa*g;        // si�a nacisku na podstaw� obiektu (na ko�a pojazdu)

	// obracam uk�ad wsp�rz�dnych lokalnych wed�ug kwaterniona orientacji:
	Wektor3 w_przod = stan.qOrient.obroc_wektor(Wektor3(1, 0, 0)); // na razie o� obiektu pokrywa si� z osi� x globalnego uk�adu wsp�rz�dnych (lokalna o� x)
	Wektor3 w_gora = stan.qOrient.obroc_wektor(Wektor3(0, 1, 0));  // wektor skierowany pionowo w g�r� od podstawy obiektu (lokalna o� y)
	Wektor3 w_prawo = stan.qOrient.obroc_wektor(Wektor3(0, 0, 1)); // wektor skierowany w prawo (lokalna o� z)

	//fprintf(f,"dlug. wekt kierunkowych: %f, %f, %f\n",w_przod.dlugosc(),w_gora.dlugosc(), w_prawo.dlugosc());

	w_przod = w_przod.znorm();
	w_gora = w_gora.znorm();
	w_prawo = w_prawo.znorm();


	// rzutujemy wV na sk�adow� w kierunku przodu i pozosta�e 2 sk�adowe
	// sk�adowa w bok jest zmniejszana przez si�� tarcia, sk�adowa do przodu
	// przez si�� tarcia tocznego
	Wektor3 wV_wprzod = w_przod*(stan.wV^w_przod),
		wV_wprawo = w_prawo*(stan.wV^w_prawo),
		wV_wgore = w_gora*(stan.wV^w_gora);

	// dodatkowa normalizacja likwidujaca blad numeryczny:
	if (stan.wV.dlugosc() > 0)
	{
		float blad_dlugosci = (wV_wprzod + wV_wprawo + wV_wgore).dlugosc() / stan.wV.dlugosc();
		wV_wprzod = wV_wprzod / blad_dlugosci;
		wV_wprawo = wV_wprawo / blad_dlugosci;
		wV_wgore = wV_wgore / blad_dlugosci;

	}

	//fprintf(f,"|wV| = %f, po rozlozeniu i zlozeniu |wV| = %f\n",wV.dlugosc(),(wV_wprzod+wV_wprawo+wV_wgore).dlugosc());


	// rzutujemy pr�dko�� k�tow� wV_kat na sk�adow� w kierunku przodu i pozosta�e 2 sk�adowe
	Wektor3 wV_kat_wprzod = w_przod*(stan.wV_kat^w_przod),
		wV_kat_wprawo = w_prawo*(stan.wV_kat^w_prawo),
		wV_kat_wgore = w_gora*(stan.wV_kat^w_gora);

	// ruch k� na skutek kr�cenia lub puszczenia kierownicy:  

	if (predkosc_krecenia_kierownica != 0)
		stan.kat_skretu_kol += predkosc_krecenia_kierownica*dt;
	else
		if (stan.kat_skretu_kol > 0)
		{
			if (!czy_kierownica_trzymana)
				stan.kat_skretu_kol -= predkosc_powrotu_kierownicy*dt;
			if (stan.kat_skretu_kol < 0) stan.kat_skretu_kol = 0;
		}
		else if (stan.kat_skretu_kol < 0)
		{
			if (!czy_kierownica_trzymana)
				stan.kat_skretu_kol += predkosc_powrotu_kierownicy*dt;
			if (stan.kat_skretu_kol > 0) stan.kat_skretu_kol = 0;
		}
	// ograniczenia: 
	if (stan.kat_skretu_kol > kat_skretu_kol_max) stan.kat_skretu_kol = kat_skretu_kol_max;
	if (stan.kat_skretu_kol < -kat_skretu_kol_max) stan.kat_skretu_kol = -kat_skretu_kol_max;
	if (F > F_max) F = F_max;
	if (F < -F_max) F = -F_max;



	Wektor3 wektor_ciazenia = (teren->srodek - stan.wPol).znorm();
	Fy = stan.masa*g*(-w_gora^wektor_ciazenia);                      // si�a docisku do pod�o�a 
	//Fy = m*g*w_gora.y;                      // si�a docisku do pod�o�a 
	if (Fy < 0) Fy = 0;
	// ... trzeba j� jeszcze uzale�ni� od tego, czy obiekt styka si� z pod�o�em!


	float V_wprzod = wV_wprzod.dlugosc();// - dt*Fh/m - dt*tarcie_toczne*Fy/m;
	//if (V_wprzod < 0) V_wprzod = 0;

	float V_wprawo = wV_wprawo.dlugosc();// - dt*tarcie*Fy/m;
	//if (V_wprawo < 0) V_wprawo = 0;







	// wjazd lub zjazd: 
	//wPol.y = teren.Wysokosc(wPol.x,wPol.z);   // najprostsze rozwi�zanie - obiekt zmienia wysoko�� bez zmiany orientacji

	// 1. gdy wjazd na wkl�s�o��: wyznaczam wysoko�ci terenu pod naro�nikami obiektu (ko�ami), 
	// sprawdzam kt�ra tr�jka
	// naro�nik�w odpowiada najni�ej po�o�onemu �rodkowi ci�ko�ci, gdy przylega do terenu
	// wyznaczam pr�dko�� podbicia (wznoszenia �rodka pojazdu spowodowanego wkl�s�o�ci�) 
	// oraz pr�dko�� k�tow�
	// 2. gdy wjazd na wypuk�o�� to si�a ci�ko�ci wywo�uje obr�t przy du�ej pr�dko�ci liniowej

	// punkty zaczepienia k� (na wysoko�ci pod�ogi pojazdu):
	//     P  o------o Q
	//        |      |
	//        |      |
	//        |      |
	//        |      |
	//        |      |
	//     R  o------o S
	Wektor3 P = stan.wPol + w_przod*(dlugosc / 2 - dl_przod) - w_prawo*szerokosc / 2 - w_gora*wysokosc / 2,
		Q = stan.wPol + w_przod*(dlugosc / 2 - dl_przod) + w_prawo*szerokosc / 2 - w_gora*wysokosc / 2,
		R = stan.wPol + w_przod*(-dlugosc / 2 + dl_tyl) - w_prawo*szerokosc / 2 - w_gora*wysokosc / 2,
		S = stan.wPol + w_przod*(-dlugosc / 2 + dl_tyl) + w_prawo*szerokosc / 2 - w_gora*wysokosc / 2;

	// pionowe rzuty punkt�w zacz. k� pojazdu na powierzchni� terenu:  
	Wektor3 Pt = teren->PunktSpadku(P), Qt = teren->PunktSpadku(Q),
		Rt = teren->PunktSpadku(R), St = teren->PunktSpadku(S);
	Wektor3 normPQR = normalna(Pt, Rt, Qt), normPRS = normalna(Pt, Rt, St), normPQS = normalna(Pt, St, Qt),
		normQRS = normalna(Qt, Rt, St);   // normalne do p�aszczyzn wyznaczonych przez tr�jk�ty


	// to jeszcze trzeba poprawi�: 

	// Czteroko�owy pojazd (o ko�ach rozmieszczonynch w wierzcho�kach kwadratu) mo�e mie� 4 (na idealnie p�askiej powierzchni)
	// lub 3 (najcz�ciej w terenie pofa�dowanym) punkty podparcia. W takim przypadku pojazdy ustawiaj� si� zwykle tak, 
	// by osi�gn�� minimum energii potencjalnej, tzn. by �rodek ci�ko�ci by� jak najbli�ej �rodka przyci�gania
	// grawitacyjnego.
	// Szukam wi�c takiego 3-punktowego podparcia, gdzie �rodek pojazdu jest najni�ej po�o�ony (s� 4 takie przypadki).
	Wektor3 S_PQR = Rt + (Pt - Rt) / 2 + (Qt - Pt) / 2 + normPQR*wysokosc / 2,     // po�o�enie �rodka pojazdu gdy punkty P,Q,R stykaj� si� z pod�o�em
		S_PRS = Rt + (Pt - Rt) / 2 + (St - Pt) / 2 + normPRS*wysokosc / 2,
		S_PQS = St + (Qt - St) / 2 + (Pt - Qt) / 2 + normPQS*wysokosc / 2,
		S_QRS = St + (Qt - St) / 2 + (Rt - St) / 2 + normQRS*wysokosc / 2;
	float sryPQR = (S_PQR - teren->srodek).dlugosc(),
		sryPRS = (S_PRS - teren->srodek).dlugosc(),
		sryPQS = (S_PQS - teren->srodek).dlugosc(),
		sryQRS = (S_QRS - teren->srodek).dlugosc();

	float sry = sryPQR; Wektor3 norm = normPQR;
	if (sry > sryPRS) { sry = sryPRS; norm = normPRS; }
	if (sry > sryPQS) { sry = sryPQS; norm = normPQS; }
	if (sry > sryQRS) { sry = sryQRS; norm = normQRS; }  // wyb�r tr�jk�ta o �rodku najni�ej po�o�onym    

	Wektor3 wV_kat_wpoziomie = Wektor3(0, 0, 0);
	// jesli kt�re� z k� jest poni�ej powierzchni terenu
	bool czy_cos_ponizej_terenu = ((P - teren->srodek).dlugosc() <= (Pt - teren->srodek).dlugosc() + wysokosc / 2 + przeswit) ||
		((Q - teren->srodek).dlugosc() <= (Qt - teren->srodek).dlugosc() + wysokosc / 2 + przeswit) ||
		((R - teren->srodek).dlugosc() <= (Rt - teren->srodek).dlugosc() + wysokosc / 2 + przeswit) ||
		((S - teren->srodek).dlugosc() <= (St - teren->srodek).dlugosc() + wysokosc / 2 + przeswit);
	if (czy_cos_ponizej_terenu)
	{
		// obliczam powsta�� pr�dko�� k�tow� w lokalnym uk�adzie wsp�rz�dnych:      
		Wektor3 wobrot = -norm.znorm()*w_gora*0.6;
		wV_kat_wpoziomie = wobrot / dt;
	}


	//Wektor3 wAg = Wektor3(0,-1,0)*g;    // przyspieszenie grawitacyjne na terenie p�askim

	Wektor3 wAg = wektor_ciazenia*g;    // przyspieszenie grawitacyjne


	// jesli wiecej niz 2 kola sa na ziemi, to przyspieszenie grawitacyjne jest rownowazone przez opor gruntu:
	int liczba_kol_na_ziemi = ((P - teren->srodek).dlugosc() <= (Pt - teren->srodek).dlugosc() + wysokosc / 2 + przeswit) +
		((Q - teren->srodek).dlugosc() <= (Qt - teren->srodek).dlugosc() + wysokosc / 2 + przeswit) +
		((R - teren->srodek).dlugosc() <= (Rt - teren->srodek).dlugosc() + wysokosc / 2 + przeswit) +
		((S - teren->srodek).dlugosc() <= (St - teren->srodek).dlugosc() + wysokosc / 2 + przeswit);
	if (liczba_kol_na_ziemi >= 3)
		wAg = wAg + w_gora*(w_gora^wAg)*-1; // odejmuj� sk�adow� przysp. w kierunku, w kt�rym �rodek pojazdu
	// nie mo�e si� przemieszacza�                             

	Fy *= (float)liczba_kol_na_ziemi / 4;
	float Fh = Fy*tarcie*ham;                  // si�a hamowania (UP: bez uwzgl�dnienia po�lizgu)


	// obliczam promien skr�tu pojazdu na podstawie k�ta skr�tu k�, a nast�pnie na podstawie promienia skr�tu
	// obliczam pr�dko�� k�tow� (UPROSZCZENIE! pomijam przyspieszenie k�towe oraz w�a�ciw� trajektori� ruchu)
	if (Fy > 0)
	{
		float V_kat_skret = 0;
		if (stan.kat_skretu_kol != 0)
		{
			float Rs = sqrt(dlugosc*dlugosc / 4 + (fabs(dlugosc / tan(stan.kat_skretu_kol)) + szerokosc / 2)*(fabs(dlugosc / tan(stan.kat_skretu_kol)) + szerokosc / 2));
			V_kat_skret = wV_wprzod.dlugosc()*(1.0 / Rs);
		}
		Wektor3 wV_kat_skret = w_gora*V_kat_skret*(stan.kat_skretu_kol > 0 ? 1 : -1);
		Wektor3 wV_kat_wgore2 = wV_kat_wgore + wV_kat_skret;
		if (wV_kat_wgore2.dlugosc() <= wV_kat_wgore.dlugosc()) // skr�t przeciwdzia�a obrotowi
		{
			if (wV_kat_wgore2.dlugosc() > V_kat_skret)
				wV_kat_wgore = wV_kat_wgore2;
			else
				wV_kat_wgore = wV_kat_skret;
		}
		else
		{
			if (wV_kat_wgore.dlugosc() < V_kat_skret)
				wV_kat_wgore = wV_kat_skret;
		}

		// tarcie zmniejsza pr�dko�� obrotow� (UPROSZCZENIE! zamiast masy winienem wykorzysta� moment bezw�adno�ci)     
		float V_kat_tarcie = Fy*tarcie_obr*dt / stan.masa / 1.0;      // zmiana pr. k�towej spowodowana tarciem
		float V_kat_wgore = wV_kat_wgore.dlugosc() - V_kat_tarcie;
		if (V_kat_wgore < V_kat_skret) V_kat_wgore = V_kat_skret;        // tarcie nie mo�e spowodowa� zmiany zwrotu wektora pr. k�towej
		wV_kat_wgore = wV_kat_wgore.znorm()*V_kat_wgore;
	}

	// sk�adam z powrotem wektor pr�dko�ci k�towej:  
	stan.wV_kat = wV_kat_wgore + wV_kat_wpoziomie;


	float h = sry + wysokosc / 2 + przeswit - stan.wPol.y;  // r�nica wysoko�ci jak� trzeba pokona�  
	//float V_podbicia = 0;

	Wektor3 dwPol = stan.wV*dt + stan.wA*dt*dt / 2; // czynnik bardzo ma�y - im wi�ksza cz�stotliwo�� symulacji, tym mniejsze znaczenie 
	stan.wPol = stan.wPol + dwPol;


	// Sprawdzenie czy obiekt mo�e si� przemie�ci� w zadane miejsce: Je�li nie, to 
	// przemieszczam obiekt do miejsca zetkni�cia, wyznaczam nowe wektory pr�dko�ci
	// i pr�dko�ci k�towej, a nast�pne obliczam nowe po�o�enie na podstawie nowych
	// pr�dko�ci i pozosta�ego czasu. Wszystko powtarzam w p�tli (pojazd znowu mo�e 
	// wjecha� na przeszkod�). Problem z zaokr�glonymi przeszkodami - konieczne 
	// wyznaczenie minimalnego kroku.

	Wektor3 wV_pop = stan.wV;

	// sk�adam pr�dko�ci w r�nych kierunkach oraz efekt przyspieszenia w jeden wektor:    (problem z przyspieszeniem od si�y tarcia -> to przyspieszenie 
	//      mo�e dzia�a� kr�cej ni� dt -> trzeba to jako� uwzgl�dni�, inaczej pojazd b�dzie w�ykowa�)
	stan.wV = wV_wprzod.znorm()*V_wprzod + wV_wprawo.znorm()*V_wprawo + wV_wgore + stan.wA*dt;
	// usuwam te sk�adowe wektora pr�dko�ci w kt�rych kierunku jazda nie jest mo�liwa z powodu
	// przesk�d:
	// np. je�li pojazd styka si� 3 ko�ami z nawierzchni� lub dwoma ko�ami i �rodkiem ci�ko�ci to
	// nie mo�e mie� pr�dko�ci w d� pod�ogi
	//if ((P.y <= Pt.y  + wysokosc/2+przeswit)||(Q.y <= Qt.y  + wysokosc/2+przeswit)||  
	//      (R.y <= Rt.y  + wysokosc/2+przeswit)||(S.y <= St.y  + wysokosc/2+przeswit))    // je�li pojazd styka si� co najm. jednym ko�em
	if (czy_cos_ponizej_terenu)
	{
		Wektor3 dwV = wV_wgore + w_gora*(stan.wA^w_gora)*dt;
		if ((w_gora.znorm() - dwV.znorm()).dlugosc() > 1)  // je�li wektor skierowany w d� pod�ogi
			stan.wV = stan.wV - dwV;
	}


	// sk�adam przyspieszenia liniowe od si� nap�dzaj�cych i od si� oporu: 
	stan.wA = (w_przod*F + w_prawo*Fb) / stan.masa*(Fy > 0)  // od si� nap�dzaj�cych
		- wV_wprzod.znorm()*(Fh / stan.masa + tarcie_toczne*Fy / stan.masa)*(V_wprzod > 0.01) // od hamowania i tarcia tocznego (w kierunku ruchu)
		- wV_wprawo.znorm()*tarcie*Fy / stan.masa/**(V_wprawo>0.01)*/    // od tarcia w kierunku prost. do kier. ruchu
		+ wAg;           // od grawitacji

	/*fprintf(f,"czy_cos_ponizej_terenu = %d, liczba_kol_na_ziemi = %d\n",czy_cos_ponizej_terenu,liczba_kol_na_ziemi);
	fprintf(f,"odl od srodka ziemi = %f (sr.promien = %f)\n",(wPol - teren->srodek).dlugosc(),teren->promien_sredni);
	fprintf(f,"|V| = %f (V w kier. srodka = %f), |A| = %f, (A w kier. srodka = %f)\n",
	wV.dlugosc(),wektor_ciazenia^wV,wA.dlugosc(),wektor_ciazenia^wA);
	fprintf(f,"W KIER. WEKT. CIAZENIA: wV_wprzod.znorm()*V_wprzod = %f, wV_wprawo.znorm()*V_wprawo = %f, wV_wgore = %f,  wA*dt = %f\n",
	(wV_wprzod.znorm()*V_wprzod)^wektor_ciazenia, (wV_wprawo.znorm()*V_wprawo)^wektor_ciazenia, wV_wgore^wektor_ciazenia, (wA*dt)^wektor_ciazenia);
	*/

	// obliczenie nowej orientacji:
	Wektor3 w_obrot = stan.wV_kat*dt + stan.wA_kat*dt*dt / 2;
	kwaternion q_obrot = AsixToQuat(w_obrot.znorm(), w_obrot.dlugosc());
	stan.qOrient = q_obrot*stan.qOrient;
}

void ObiektRuchomy::Rysuj()
{
	glPushMatrix();


	glTranslatef(stan.wPol.x, stan.wPol.y + przeswit, stan.wPol.z);

	Wektor3 k = stan.qOrient.AsixAngle();     // reprezentacja k�towo-osiowa kwaterniona

	Wektor3 k_znorm = k.znorm();

	glRotatef(k.dlugosc()*180.0 / PI, k_znorm.x, k_znorm.y, k_znorm.z);
	glTranslatef(-dlugosc / 2, -wysokosc / 2, -szerokosc / 2);
	glScalef(dlugosc, wysokosc, szerokosc);

	glCallList(Auto);
	GLfloat Surface[] = { 2.0f, 2.0f, 1.0f, 1.0f };
	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, Surface);
	if (czy_rysowac_ID){
		glRasterPos2f(0.30, 1.20);
		glPrint("%d", iID);
	}
	glPopMatrix();
}




//*************************
//   Obiekty nieruchome
//*************************
Teren::Teren()
{
	char nazwa[128], nazwa_pliku[128], nazwa_pliku2[128];
	strcpy(nazwa, "planeta3");
	sprintf(nazwa_pliku, "%s.obj", nazwa);
	sprintf(nazwa_pliku2, "..//%s.obj", nazwa);
	int wynik = tworz_z_obj(&siatka, nazwa_pliku);      // wczytanie siatki t�jk�t�w z formatu *.obj (Wavefront)
	if (wynik == 0)
		int wynik = tworz_z_obj(&siatka, nazwa_pliku2);      // wczytanie siatki t�jk�t�w z formatu *.obj (Wavefront)

	float skala = 2000.0;

	fprintf(f, "liczba wezlow = %d, liczba scian = %d\n", siatka.liczba_wezlow, siatka.liczba_scian);

	for (long i = 0; i < siatka.liczba_wezlow; i++)              // przeskalowanie siatki
		siatka.wezly[i].wPol = siatka.wezly[i].wPol*skala;




	for (long i = 0; i < siatka.liczba_scian; i++)               // wyznaczenie normalnych do poszcz. �cian i wierzcho�k�w
	{
		Wektor3 A = siatka.wezly[siatka.sciany[i].ind1].wPol, B = siatka.wezly[siatka.sciany[i].ind2].wPol,
			C = siatka.wezly[siatka.sciany[i].ind3].wPol;

		siatka.sciany[i].normalna = normalna(A, B, C);         // normalna do trojkata ABC
		siatka.wezly[siatka.sciany[i].ind1].normalna = siatka.wezly[siatka.sciany[i].ind2].normalna =
			siatka.wezly[siatka.sciany[i].ind3].normalna = siatka.sciany[i].normalna;  // normalna do wyg�adzania
	}

	// Wyznaczenie �rodka ci�ko�ci -> �rodka przyci�gania grawitacyjnego
	float x_min = 1e10, x_max = -1e10, y_min = 1e10, y_max = -1e10, z_min = 1e10, z_max = -1e10;
	for (long i = 0; i < siatka.liczba_scian; i++)
	{
		Wektor3 P = siatka.wezly[siatka.sciany[i].ind1].wPol;
		if (x_min > P.x) x_min = P.x;
		if (x_max < P.x) x_max = P.x;
		if (y_min > P.y) y_min = P.y;
		if (y_max < P.y) y_max = P.y;
		if (z_min > P.z) z_min = P.z;
		if (z_max < P.z) z_max = P.z;
	}
	srodek = Wektor3((x_max + x_min) / 2, (y_max + y_min) / 2, (z_max + z_min) / 2);

	// Wyznaczenie �redniego promienia kuli:
	promien_sredni = 0;
	for (long i = 0; i < siatka.liczba_wezlow; i++)
		promien_sredni += (siatka.wezly[i].wPol - srodek).dlugosc() / siatka.liczba_wezlow;


	rozmiar_sektora = (x_max - x_min) / 20;   // rozmiar sektora sze�ciennego 

	tab_sekt = new TabSektorow(10000);

	// Umieszczenie �cian w sektorach, a sektor�w w tablicy rozproszonej, by szybko je wyszukiwa�:
	for (long i = 0; i < siatka.liczba_scian; i++)
	{
		float x_min = 1e10, x_max = -1e10, y_min = 1e10, y_max = -1e10, z_min = 1e10, z_max = -1e10;  // granice przedzia��w, w kt�rych mie�ci si� �ciana
		long e[] = { siatka.sciany[i].ind1, siatka.sciany[i].ind2, siatka.sciany[i].ind3 };
		Wektor3 N = siatka.sciany[i].normalna, P = siatka.wezly[e[0]].wPol,
			E0 = siatka.wezly[e[0]].wPol, E1 = siatka.wezly[e[1]].wPol, E2 = siatka.wezly[e[2]].wPol;

		for (long j = 0; j < 3; j++)      // po wierzcho�kach tr�jk�ta
		{
			Wektor3 P = siatka.wezly[e[j]].wPol;
			if (x_min > P.x) x_min = P.x;
			if (x_max < P.x) x_max = P.x;
			if (y_min > P.y) y_min = P.y;
			if (y_max < P.y) y_max = P.y;
			if (z_min > P.z) z_min = P.z;
			if (z_max < P.z) z_max = P.z;
		}
		// wyznaczenie indeks�w sektor�w dla granic przedzia��w:
		long ind_x_min = (long)floor(x_min / rozmiar_sektora), ind_x_max = (long)floor(x_max / rozmiar_sektora),
			ind_y_min = (long)floor(y_min / rozmiar_sektora), ind_y_max = (long)floor(y_max / rozmiar_sektora),
			ind_z_min = (long)floor(z_min / rozmiar_sektora), ind_z_max = (long)floor(z_max / rozmiar_sektora);

		//fprintf(f,"i=%d, ind_x_min = %d, ind_x_max = %d\n",i,ind_x_min,ind_x_max);

		// skanowanie po wszystkich sektorach prostopad�o�cianu, w kt�rym znajduje si� tr�jk�t by sprawdzi� czy jaki�
		// fragment tr�jk�ta znajduje si� w sektorze. Je�li tak, to sektor umieszczany jest w tablicy rozproszonej:
		for (long x = ind_x_min; x <= ind_x_max; x++)
			for (long y = ind_y_min; y <= ind_y_max; y++)
				for (long z = ind_z_min; z <= ind_z_max; z++)
				{
					bool czy_zawiera = 0;
					float x1 = (float)x*rozmiar_sektora, x2 = (float)(x + 1)*rozmiar_sektora,  // granice sektora
						y1 = (float)y*rozmiar_sektora, y2 = (float)(y + 1)*rozmiar_sektora,
						z1 = (float)z*rozmiar_sektora, z2 = (float)(z + 1)*rozmiar_sektora;

					// sprawdzenie czy wewn�trz sze�cianu sektora nie ma kt�rego� z 3 wierzcho�k�w: 
					for (long j = 0; j < 3; j++)      // po wierzcho�kach tr�jk�ta
					{
						Wektor3 P = siatka.wezly[e[j]].wPol;
						if ((P.x >= x1) && (P.x <= x2) && (P.y >= y1) && (P.y <= y2) && (P.z >= z1) && (P.z <= z2))
						{
							czy_zawiera = 1;
							break;
						}
					}

					// sprawdzenie, czy kt�ry� z 12 bok�w sze�cianu nie przecina �ciany:
					if (czy_zawiera == 0)
					{

						// zbi�r par wierzcho�k�w definiuj�cych boki:
						Wektor3 AB[][2] = { { Wektor3(x1, y1, z1), Wektor3(x2, y1, z1) }, { Wektor3(x1, y2, z1), Wektor3(x2, y2, z1) },  // w kier. osi x
						{ Wektor3(x1, y1, z2), Wektor3(x2, y1, z2) }, { Wektor3(x1, y2, z2), Wektor3(x2, y2, z2) },
						{ Wektor3(x1, y1, z1), Wektor3(x1, y2, z1) }, { Wektor3(x2, y1, z1), Wektor3(x2, y2, z1) },  // w kier. osi y
						{ Wektor3(x1, y1, z2), Wektor3(x1, y2, z2) }, { Wektor3(x2, y1, z2), Wektor3(x2, y2, z2) },
						{ Wektor3(x1, y1, z1), Wektor3(x1, y1, z2) }, { Wektor3(x2, y1, z1), Wektor3(x2, y1, z2) },  // w kier. osi z
						{ Wektor3(x1, y2, z1), Wektor3(x1, y2, z2) }, { Wektor3(x2, y2, z1), Wektor3(x2, y2, z2) } };

						for (long j = 0; j < 12; j++)
						{
							Wektor3 A = AB[j][0], B = AB[j][1];
							Wektor3 X = punkt_przec_prostej_z_plaszcz(A, B, N, P);
							if (czy_w_trojkacie(E0, E1, E2, X) && (((X - A) ^ (X - B)) <= 0)){
								czy_zawiera = 1;
								break;
							}
						}
					}

					// sprawdzenie, czy kt�ry� z 3 bok�w tr�jk�ta nie przecina kt�rej� z 6 �cian sze�cianu:
					if (czy_zawiera == 0)
					{
						Wektor3 AB[][2] = { { E0, E1 }, { E1, E2 }, { E2, E0 } };
						for (long j = 0; j < 3; j++) // po bokach tr�jk�ta
						{
							Wektor3 A = AB[j][0], B = AB[j][1];
							Wektor3 X = punkt_przec_prostej_z_plaszcz(A, B, Wektor3(1, 0, 0), Wektor3(x1, y1, z1));     // p�aszczyzna x = x1
							if ((X.y >= y1) && (X.y <= y2) && (X.z >= z1) && (X.z <= z2)) { czy_zawiera = 1; break; }
							X = punkt_przec_prostej_z_plaszcz(A, B, Wektor3(1, 0, 0), Wektor3(x2, y1, z1));             // p�aszczyzna x = x2
							if ((X.y >= y1) && (X.y <= y2) && (X.z >= z1) && (X.z <= z2)) { czy_zawiera = 1; break; }
							X = punkt_przec_prostej_z_plaszcz(A, B, Wektor3(0, 1, 0), Wektor3(x1, y1, z1));             // p�aszczyzna y = y1
							if ((X.x >= x1) && (X.x <= x2) && (X.z >= z1) && (X.z <= z2)) { czy_zawiera = 1; break; }
							X = punkt_przec_prostej_z_plaszcz(A, B, Wektor3(0, 1, 0), Wektor3(x1, y2, z1));             // p�aszczyzna y = y2
							if ((X.x >= x1) && (X.x <= x2) && (X.z >= z1) && (X.z <= z2)) { czy_zawiera = 1; break; }
							X = punkt_przec_prostej_z_plaszcz(A, B, Wektor3(0, 0, 1), Wektor3(x1, y1, z1));             // p�aszczyzna z = z1
							if ((X.x >= x1) && (X.x <= x2) && (X.y >= y1) && (X.y <= y2)) { czy_zawiera = 1; break; }
							X = punkt_przec_prostej_z_plaszcz(A, B, Wektor3(0, 0, 1), Wektor3(x1, y1, z2));             // p�aszczyzna z = z2
							if ((X.x >= x1) && (X.x <= x2) && (X.y >= y1) && (X.y <= y2)) { czy_zawiera = 1; break; }
						}

					}

					if (czy_zawiera)
					{
						SektorObiektu *sektor = tab_sekt->znajdz(x, y, z);
						if (sektor == NULL)
						{
							SektorObiektu sek;
							sek.liczba_scian = 1;
							sek.rozmiar_pamieci = 1;
							sek.sciany = new Sciana*[1];
							sek.sciany[0] = &siatka.sciany[i];
							sek.x = x; sek.y = y; sek.z = z;
							tab_sekt->wstaw(sek);
						}
						else
						{
							if (sektor->liczba_scian >= sektor->rozmiar_pamieci)
							{
								long nowy_rozmiar = sektor->rozmiar_pamieci * 2;
								Sciana **sciany2 = new Sciana*[nowy_rozmiar];
								for (long j = 0; j < sektor->liczba_scian; j++) sciany2[j] = sektor->sciany[j];
								delete sektor->sciany;
								sektor->sciany = sciany2;
								sektor->rozmiar_pamieci = nowy_rozmiar;
							}
							sektor->sciany[sektor->liczba_scian] = &siatka.sciany[i];
							sektor->liczba_scian++;
						}
					}


				} // 3x for po sektorach prostopad�o�cianu 

	} // for po �cianach
	fprintf(f, "floor(-1.1) = %f, MAX_LONG/2 = %d\n", floor(-1.1), LONG_MAX / 2);

	fprintf(f, "srodek sfery = (%f, %f, %f)\n", srodek.x, srodek.y, srodek.z);

	long liczba_sektorow = 0, liczba_wsk_scian = 0;
	for (long i = 0; i < tab_sekt->liczba_komorek; i++)
	{
		//fprintf(f,"komorka %d zaweira %d sektorow\n",i,tab_sekt->komorki[i].liczba_sektorow);
		liczba_sektorow += tab_sekt->komorki[i].liczba_sektorow;
		for (long j = 0; j < tab_sekt->komorki[i].liczba_sektorow; j++)
		{
			//fprintf(f,"  sektor %d (%d, %d, %d) zawiera %d scian\n",j,tab_sekt->komorki[i].sektory[j].x,
			//  tab_sekt->komorki[i].sektory[j].y, tab_sekt->komorki[i].sektory[j].z,
			//  tab_sekt->komorki[i].sektory[j].liczba_scian);
			liczba_wsk_scian += tab_sekt->komorki[i].sektory[j].liczba_scian;
		}
	}
	//fprintf(f,"laczna liczba sektorow = %d, wskaznikow scian = %d\n",liczba_sektorow, liczba_wsk_scian);

	//rozmiar_sektora = 10;
	//SektorObiektu **s = NULL;
	//long liczba_s = SektoryWkierunku(&s, Wektor3(35,-5,0), Wektor3(-15,35,0));
	//long liczba_s = SektoryWkierunku(&s, Wektor3(33,55,0),Wektor3(5,5,0) );
}

Teren::~Teren()
{
	delete siatka.wezly; delete siatka.sciany;
}

// Wyznaczanie sektor�w przecinanych przez wektor o podanym punkcie startowym i ko�cowym
// zwraca liczb� znalezionych sektor�w
long Teren::SektoryWkierunku(SektorObiektu ***wsk_sekt, Wektor3 punkt_startowy, Wektor3 punkt_koncowy)
{
	long liczba_sektorow = 0;
	long liczba_max = 10;
	SektorObiektu **s = new SektorObiektu*[liczba_max];

	// wyznaczenie wsp�rz�dnych pocz�tku sektora startowego i ko�cowego:
	long x_start = (long)floor(punkt_startowy.x / rozmiar_sektora), y_start = (long)floor(punkt_startowy.y / rozmiar_sektora),
		z_start = (long)floor(punkt_startowy.z / rozmiar_sektora);
	long x_koniec = (long)floor(punkt_koncowy.x / rozmiar_sektora), y_koniec = (long)floor(punkt_koncowy.y / rozmiar_sektora),
		z_koniec = (long)floor(punkt_koncowy.z / rozmiar_sektora);

	float krok = (x_start < x_koniec ? 1 : -1);
	for (long x = x_start; x != x_koniec; x += krok)
	{
		float xf = (float)(x + krok + (krok == -1))*rozmiar_sektora;   // wsp. x punktu przeciecia wektora z g�rn� p�aszczyzn� sektora
		// wyznaczenie punktu przeci�cia wektora z kolejn� p�aszczyzn� w kierunku osi x (dla indeksu x):
		Wektor3 P = punkt_przec_prostej_z_plaszcz(punkt_startowy, punkt_koncowy, Wektor3(1, 0, 0), Wektor3(xf, 0, 0));
		// wyznaczenie indeks�w sektora, kt�rego g�rna p�aszczyzna jest przecinana: 
		long y = (long)floor(P.y / rozmiar_sektora), z = (long)floor(P.z / rozmiar_sektora);
		// dodanie sektora do listy:
		SektorObiektu *sektor = tab_sekt->znajdz(x, y, z);
		//fprintf(f,"dla x = %d, sektor (%d, %d, %d)\n",x,x,y,z);
		if (sektor){
			s[liczba_sektorow] = sektor;
			liczba_sektorow++;
		}
		if (liczba_max <= liczba_sektorow)
		{
			long nowa_liczba_max = liczba_max * 2;
			SektorObiektu **s2 = new SektorObiektu*[nowa_liczba_max];
			for (long i = 0; i < liczba_sektorow; i++) s2[i] = s[i];
			delete s; s = s2; liczba_max = nowa_liczba_max;
		}
	}
	krok = (y_start < y_koniec ? 1 : -1);
	for (long y = y_start; y != y_koniec; y += krok)
	{
		float yf = (float)(y + krok + (krok == -1))*rozmiar_sektora;   // wsp. x punktu przeciecia wektora z g�rn� p�aszczyzn� sektora
		// wyznaczenie punktu przeci�cia wektora z kolejn� p�aszczyzn� w kierunku osi x (dla indeksu x):
		Wektor3 P = punkt_przec_prostej_z_plaszcz(punkt_startowy, punkt_koncowy, Wektor3(0, 1, 0), Wektor3(0, yf, 0));
		// wyznaczenie indeks�w sektora, kt�rego g�rna p�aszczyzna jest przecinana: 
		long x = (long)floor(P.x / rozmiar_sektora), z = (long)floor(P.z / rozmiar_sektora);
		// dodanie sektora do listy:

		SektorObiektu *sektor = tab_sekt->znajdz(x, y, z);
		//fprintf(f,"dla y = %d, sektor (%d, %d, %d)\n",y,x,y,z);
		if (sektor){
			s[liczba_sektorow] = sektor;
			liczba_sektorow++;
		}
		if (liczba_max <= liczba_sektorow)
		{
			long nowa_liczba_max = liczba_max * 2;
			SektorObiektu **s2 = new SektorObiektu*[nowa_liczba_max];
			for (long i = 0; i < liczba_sektorow; i++) s2[i] = s[i];
			delete s; s = s2; liczba_max = nowa_liczba_max;
		}
	}
	krok = (z_start < z_koniec ? 1 : -1);
	for (long z = z_start; z != z_koniec; z += krok)
	{
		float zf = (float)(z + krok + (krok == -1))*rozmiar_sektora;   // wsp. x punktu przeciecia wektora z g�rn� p�aszczyzn� sektora
		// wyznaczenie punktu przeci�cia wektora z kolejn� p�aszczyzn� w kierunku osi x (dla indeksu x):
		Wektor3 P = punkt_przec_prostej_z_plaszcz(punkt_startowy, punkt_koncowy, Wektor3(0, 0, 1), Wektor3(0, 0, zf));
		// wyznaczenie indeks�w sektora, kt�rego g�rna p�aszczyzna jest przecinana: 
		long x = (long)floor(P.x / rozmiar_sektora), y = (long)floor(P.y / rozmiar_sektora);
		// dodanie sektora do listy:
		SektorObiektu *sektor = tab_sekt->znajdz(x, y, z);
		//fprintf(f,"dla z = %d, sektor (%d, %d, %d)\n",z,x,y,z);
		if (sektor){
			s[liczba_sektorow] = sektor;
			liczba_sektorow++;
		}
		if (liczba_max <= liczba_sektorow)
		{
			long nowa_liczba_max = liczba_max * 2;
			SektorObiektu **s2 = new SektorObiektu*[nowa_liczba_max];
			for (long i = 0; i < liczba_sektorow; i++) s2[i] = s[i];
			delete s; s = s2; liczba_max = nowa_liczba_max;
		}
	}
	SektorObiektu *sektor = tab_sekt->znajdz(x_koniec, y_koniec, z_koniec);
	//fprintf(f,"punkt koncowy, sektor (%d, %d, %d)\n",x_koniec,y_koniec,z_koniec);
	if (sektor){
		s[liczba_sektorow] = sektor;
		liczba_sektorow++;
	}

	(*wsk_sekt) = s;

	return liczba_sektorow;
}

Wektor3 Teren::PunktSpadku(Wektor3 P)     // okre�lanie wysoko�ci dla punktu P (odleg�o�ci od najbli�szej p�aszczyzny w kierunku grawitacji)
{                                         // 
	float odl_min = 1e10;
	Wektor3 Pp_min = this->srodek;          // �rodek bry�y
	long ind_min = 0;
	SektorObiektu **s = NULL;               // tablica wska�nik�w sektor�w                                   
	long liczba_s = SektoryWkierunku(&s, P, this->srodek); // wyszukanie sektor�w przecinanych przez wektor P_srodek, w kt�rych znajduj� si� �ciany 
	for (long j = 0; j < liczba_s; j++)           // po niepustych sektorach
	{
		for (long i = 0; i<s[j]->liczba_scian; i++)  // po �cianach z sektora j-tego
		{
			Wektor3 A = siatka.wezly[s[j]->sciany[i]->ind1].wPol, B = siatka.wezly[s[j]->sciany[i]->ind2].wPol,
				C = siatka.wezly[s[j]->sciany[i]->ind3].wPol;  // po�o�enia wierzcho�k�w tr�jk�ta �ciany
			Wektor3 Pp = punkt_przec_prostej_z_plaszcz(P, this->srodek, s[j]->sciany[i]->normalna, A); // punkt przeci�cia wektora P_srodek z p�aszczyzn� j-tej �ciany
			bool czy_w_tr = czy_w_trojkacie(A, B, C, Pp);     // sprawdzenie czy punkt przeci�cia znajduje si� wewn�trz tr�jk�ta �ciany
			if (czy_w_tr)                                     // je�li punkt przeci�cia jest wewn�trz tr�jk�ta �ciany
			{                                                 // to sprawdzenie, czy punkt ten le�y najbli�ej od punktu P
				float odl_od_plasz = (P - Pp).dlugosc();
				float odl_od_srodka = (P - srodek).dlugosc();
				if ((odl_od_srodka >= odl_od_plasz) && (odl_min > odl_od_plasz))
				{
					odl_min = odl_od_plasz;
					ind_min = i;
					Pp_min = Pp;
				}
			}
		} // for po �cianach
	} // for po sektorach
	delete s;

	return Pp_min;
}

Wektor3 Teren::PunktMax(Wektor3 v)     // okre�lanie punktu le��cego jak najdalej od �rodka sfery w kierunku v
{
	float odl_max = 0;
	Wektor3 Pp_max = this->srodek;
	long ind_max = 0;
	for (long i = 0; i < siatka.liczba_scian; i++)
	{
		Wektor3 A = siatka.wezly[siatka.sciany[i].ind1].wPol, B = siatka.wezly[siatka.sciany[i].ind2].wPol,
			C = siatka.wezly[siatka.sciany[i].ind3].wPol;
		Wektor3 Pp = punkt_przec_prostej_z_plaszcz(this->srodek, this->srodek + v, siatka.sciany[i].normalna, A);
		bool czy_w_tr = czy_w_trojkacie(A, B, C, Pp);


		if ((czy_w_tr) && (((Pp - srodek) ^ v) > 0)) // czy punkt w tr�jk�cie i w kierunku v 
		{

			float odl_od_srodka = (Pp - srodek).dlugosc();
			if (odl_max < odl_od_srodka)
			{
				odl_max = odl_od_srodka;
				ind_max = i;
				Pp_max = Pp;
			}
			fprintf(f, "sciana %d: A(%f,%f,%f), B(%f,%f,%f), C(%f,%f,%f), Pp = (%f,%f,%f), czy_w_tr =%d\n", i, A.x, A.y, A.z, B.x, B.y, B.z, C.x, C.y, C.z,
				Pp.x, Pp.y, Pp.z, czy_w_tr);
			fprintf(f, " odl. od srodka = %f\n", odl_od_srodka);
		}

	}

	return Pp_max;
}

void Teren::PoczatekGrafiki()
{
	bool wygladzanie = 0;
	glNewList(PowierzchniaTerenu, GL_COMPILE);
	glBegin(GL_TRIANGLES);

	for (long i = 0; i < siatka.liczba_scian; i++)
	{
		Wektor3 A = siatka.wezly[siatka.sciany[i].ind1].wPol, B = siatka.wezly[siatka.sciany[i].ind2].wPol,
			C = siatka.wezly[siatka.sciany[i].ind3].wPol;
		Wektor3 n = siatka.sciany[i].normalna;
		if (wygladzanie) n = siatka.wezly[siatka.sciany[i].ind1].normalna;
		glNormal3f(n.x, n.y, n.z);
		glVertex3f(A.x, A.y, A.z);
		if (wygladzanie) n = siatka.wezly[siatka.sciany[i].ind2].normalna;
		glNormal3f(n.x, n.y, n.z);
		glVertex3f(B.x, B.y, B.z);
		if (wygladzanie) n = siatka.wezly[siatka.sciany[i].ind3].normalna;
		glNormal3f(n.x, n.y, n.z);
		glVertex3f(C.x, C.y, C.z);
	}
	glEnd();
	glEndList();
}



void Teren::Rysuj()
{
	glCallList(PowierzchniaTerenu);
}


TabSektorow::TabSektorow(long liczba_kom)
{
	liczba_komorek = liczba_kom;
	komorki = new KomorkaTablicy[liczba_komorek];
	for (long i = 0; i < liczba_komorek; i++)
	{
		komorki[i].liczba_sektorow = 0;
		komorki[i].rozmiar_pamieci = 0;
		komorki[i].sektory = NULL;
	}
}

TabSektorow::~TabSektorow()
{
	for (long i = 0; i < liczba_komorek; i++)
		delete komorki[i].sektory;
	delete komorki;
}

long TabSektorow::wyznacz_klucz(long x, long y, long z)                  // wyznaczanie indeksu kom�rki 
{
	long kl = (long)fabs((cos(x*1.0)*cos(y*1.1)*cos(z*1.2) + cos(x*1.0) + cos(y*1.0) + cos(z*1.0))*liczba_komorek * 10) % liczba_komorek;
	return kl;
}

SektorObiektu *TabSektorow::znajdz(long x, long y, long z)       // znajdowanie sektora (zwraca NULL je�li nie znaleziono)
{
	long klucz = wyznacz_klucz(x, y, z);
	SektorObiektu *sektor = NULL;
	for (long i = 0; i < komorki[klucz].liczba_sektorow; i++)
	{
		SektorObiektu *s = &komorki[klucz].sektory[i];
		if ((s->x == x) && (s->y == y) && (s->z == z)) {
			sektor = s;
			break;
		}
	}
	return sektor;
}

SektorObiektu *TabSektorow::wstaw(SektorObiektu s)      // wstawianie sektora do tablicy
{
	long x = s.x, y = s.y, z = s.z;
	long klucz = wyznacz_klucz(x, y, z);
	long liczba_sekt = komorki[klucz].liczba_sektorow;
	if (liczba_sekt >= komorki[klucz].rozmiar_pamieci)     // jesli brak pamieci, to nale�y j� powi�kszy� 
	{
		long nowy_rozmiar = (komorki[klucz].rozmiar_pamieci == 0 ? 1 : komorki[klucz].rozmiar_pamieci * 2);
		SektorObiektu *sektory2 = new SektorObiektu[nowy_rozmiar];
		for (long i = 0; i < komorki[klucz].liczba_sektorow; i++) sektory2[i] = komorki[klucz].sektory[i];
		delete komorki[klucz].sektory;
		komorki[klucz].sektory = sektory2;
		komorki[klucz].rozmiar_pamieci = nowy_rozmiar;
	}
	komorki[klucz].sektory[liczba_sekt] = s;
	komorki[klucz].liczba_sektorow++;

	return &komorki[klucz].sektory[liczba_sekt];           // zwraca wska�nik do nowoutworzonego sektora
}

/*
	Wczytywanie eksportowanego z Blendera formatu obj, w ktorym zapisywane sa wierzcholki i sciany
	Zwraca 1 w razie powodzenia, a 0 gdy nie da si� odczyta� pliku
	*/
int tworz_z_obj(SiatkaObiektu *si, char nazwa_pliku[])
{
	FILE *f = fopen(nazwa_pliku, "r");

	if (f == NULL)
		return 0;

	char lan[1024], napis[128], s1[128], s2[128], s3[128], s4[128];

	long liczba_max_wezlow = 100;
	long liczba_max_scian = 100;

	si->wezly = new Wezel[liczba_max_wezlow];
	si->liczba_wezlow = 0;
	si->sciany = new Sciana[liczba_max_scian];
	si->liczba_scian = 0;

	long lw = si->liczba_wezlow, ls = si->liczba_scian;

	while (fgets(lan, 1024, f))
	{
		sscanf(lan, "%s", &napis);

		if (strcmp(napis, "v") == 0)     // znaleziono dane wierzcholka -> tworze nowa kule
		{
			sscanf(lan, "%s %s %s %s", &napis, &s1, &s2, &s3);
			si->wezly[lw].wPol = Wektor3(atof(s1), atof(s2), atof(s3));

			lw++;  si->liczba_wezlow = lw;

			if (si->liczba_wezlow >= liczba_max_wezlow)  // jesli pamiec na kule wyczerpala sie
			{
				liczba_max_wezlow *= 2;
				Wezel *wezly_pom = si->wezly;
				si->wezly = new Wezel[liczba_max_wezlow];
				for (long i = 0; i < si->liczba_wezlow; i++) si->wezly[i] = wezly_pom[i];
				delete wezly_pom;
			}
		}

		if (strcmp(napis, "f") == 0)  // znaleziono trojkat (lub czworokat) -> tworze nowa scianke 
		{
			int wynik = sscanf(lan, "%s %s %s %s %s", &napis, &s1, &s2, &s3, &s4);
			int i1 = atoi(s1) - 1, i2 = atoi(s2) - 1, i3 = atoi(s3) - 1;  // w pliku *.obj wezly indeksowane sa od 1, stad trzeba odjac 1 by przejsc na ind. od 0
			si->sciany[ls].ind1 = i1; si->sciany[ls].ind2 = i2; si->sciany[ls].ind3 = i3;
			ls++; si->liczba_scian = ls;

			if (wynik == 5)  // jesli siatka czworokatow a nie trojkatow
			{
				int i4 = atoi(s4) - 1;
				si->sciany[ls].ind1 = i3; si->sciany[ls].ind2 = i4; si->sciany[ls].ind3 = i1;
				ls++; si->liczba_scian = ls;

			}
			if (si->liczba_scian >= liczba_max_scian)  // jesli pamiec na kule wyczerpala sie
			{
				liczba_max_scian *= 2;
				Sciana *sciany_pom = si->sciany;
				si->sciany = new Sciana[liczba_max_scian];
				for (long i = 0; i < si->liczba_scian; i++) si->sciany[i] = sciany_pom[i];
				delete sciany_pom;
			}

		}

	} // while po wierszach pliku

	fclose(f);

	return 1;
}