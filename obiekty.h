#include <stdio.h>
#include "kwaternion.h"

#define PI 3.1416

struct StanObiektu
{
	//int iID;                  // identyfikator obiektu
	Wektor3 wPol;             // polozenie obiektu (�rodka geometrycznego obiektu) 
	kwaternion qOrient;       // orientacja (polozenie katowe)
	Wektor3 wV, wA;            // predkosc, przyspiesznie liniowe
	Wektor3 wV_kat, wA_kat;   // predkosc i przyspieszenie liniowe
	float masa;
	float kat_skretu_kol;
};


struct Wezel
{
	Wektor3 wPol;         // po�o�enie w�z�a
	Wektor3 normalna;     // normalna w trybie wyg�adzania
};

struct Sciana
{
	long ind1, ind2, ind3;  // indeksy w�z��w
	Wektor3 normalna;     // wektor prostopadly do  plaszczyzny trojkata
};

struct SiatkaObiektu    // siatka tr�jk�t�w
{
	Wezel *wezly;
	Sciana *sciany;
	long liczba_wezlow, liczba_scian;
};

struct SektorObiektu   // sektor zawieraj�cy �ciany  
{
	long x, y, z;           // wsp�rz�dne pocz�tku sektora
	Sciana **sciany;      // tablica wska�nik�w do �cian   
	long liczba_scian;    // liczba �cian w sektorze
	long rozmiar_pamieci; // liczba �cian, dla kt�rej zarezerwowano pami��  
};

struct KomorkaTablicy  // kom�rka tablicy rozproszonej (te� mog�aby by� tablic� rozproszon�)
{
	SektorObiektu *sektory;
	long liczba_sektorow;
	long rozmiar_pamieci; // liczba sektor�w, dla kt�rej zarezerwowano pami��  
};

class TabSektorow      // tablica rozproszona przechowuj�ca sektory
{
	//private:
public:
	long liczba_komorek;                                 // liczba kom�rek tablicy
	KomorkaTablicy *komorki;
	long wyznacz_klucz(long x, long y, long z);          // wyznaczanie indeksu kom�rki 

public:
	TabSektorow(long liczba_kom);                        // na wej�ciu konstruktora liczba kom�rek tablicy
	~TabSektorow();

	SektorObiektu *znajdz(long x, long y, long z);       // znajdowanie sektor�w (zwraca NULL je�li nie znaleziono)
	SektorObiektu *wstaw(SektorObiektu s);               // wstawianie sektor�w do tablicy
};

// Klasa opisuj�ca teren, po kt�rym poruszaj� si� obiekty
class Teren
{
public:
	SiatkaObiektu siatka;    // siatka w�z��w i �cian, z kt�rych sk�ada si� teren (mo�e by� dowolnego kszta�tu)
	Wektor3 srodek;          // �rodek przyci�gania grawitacyjnego
	float promien_sredni;    // promie� kuli
	TabSektorow *tab_sekt;    // tablica rozproszona do przechowywania sektor�w
	float rozmiar_sektora;    // rozmiar sektora sze�ciennego

	Teren();
	~Teren();

	long SektoryWkierunku(SektorObiektu ***wsk_sekt, Wektor3 punkt_startowy, Wektor3 punkt_koncowy);  // zbi�r sektor�w, przez kt�ry przechodzi podany wektor
	Wektor3 PunktSpadku(Wektor3 P);     // okre�lanie rzutu P na teren w kierunku dzia�ania si�y ci��enia
	Wektor3 PunktMax(Wektor3 v);        // okre�lanie punktu le��cego jak najdalej od �rodka sfery w kierunku v
	void Rysuj();	                      // odrysowywanie terenu   
	void PoczatekGrafiki();             // tworzenie listy wy�wietlania
};


// Klasa opisuj�ca obiekty ruchome
class ObiektRuchomy
{
public:
	int iID;                  // identyfikator obiektu
	
	StanObiektu stan;

	float F, Fb;               // si�y dzia�aj�ce na obiekt: F - pchajaca do przodu, Fb - w prawo
	float ham;                // stopie� hamowania Fh_max = tarcie*Fy*ham
	float F_max;              // maksymalna si�a nap�dowa
	float predkosc_krecenia_kierownica;      // pr�dko�� kr�cenia kierownic�
	bool czy_kierownica_trzymana;

	//float alfa;               // kat skretu kol w radianach (w lewo - dodatni)
	float kat_skretu_kol_max;           // maksymalny k�t skr�tu
	

	//float m;				  // masa obiektu	
	float masa_wlasna;        // masa w�asna obiektu [kg] (bez paliwa)
	float ilosc_paliwa;       // masa paliwa [kg]
	
	float dlugosc, szerokosc, wysokosc; // rozmiary w kierunku lokalnych osi x,y,z
	float przeswit;           // wysoko�� na kt�rej znajduje si� podstawa obiektu
	float dl_przod;           // odleg�o�� od przedniej osi do przedniego zderzaka 
	float dl_tyl;             // odleg�o�� od tylniej osi do tylniego zderzaka
	float predkosc_powrotu_kierownicy;    // pr�dko�� powrotu kierownicy po puszczeniu
	Teren *teren;             // wska�nik do terenu

public:
	float promien;

	int iID_kolid;            // identyfikator pojazdu z kt�rym nast�pi�a kolizja (-1  brak kolizji)
	Wektor3 wdV_kolid;        // poprawka pr�dko�ci pojazdu koliduj�cego

	ObiektRuchomy();            // konstruktor
	ObiektRuchomy(Teren* teren); // konstruktor z umiejscowieniem w terenie
	~ObiektRuchomy();
	void ZmienStan(StanObiektu stan);          // zmiana stanu obiektu
	StanObiektu Stan();        // metoda zwracajaca stan obiektu
	void Symulacja(float dt);  // symulacja ruchu obiektu w oparciu o biezacy stan, przylozone sily
	// oraz czas dzialania sil. Efektem symulacji jest nowy stan obiektu 
	void Rysuj();			   // odrysowanie obiektu					
};



/*
	Wczytywanie eksportowanego z Blendera formatu obj (Wavefront), w ktorym zapisywane sa wierzcholki i sciany
	Zwraca 1 w razie powodzenia
	*/
int tworz_z_obj(SiatkaObiektu *si, char nazwa_pliku[]);