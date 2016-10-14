#include "definitions.h"


/////////////////////////////////////////////////////////////////////////////////////////////////////////
///		Файл definitions.cpp - вспомогательные функции
///
///						ИНТЕРПРЕТАТОР 1.0
///
///
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern bool error;

//================ ПОЖКЛЮЧЕНИЕ СТАНДАРТНЫХ ЗАГОЛОВОЧНЫХ ФАЙЛОВ =============================================
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <math.h>


double ToRadians (double corner)
{
    return M_PI * corner / 180.0;
}

double ToGrad (double corner)
{
    return corner * 180.0 / M_PI;
}

double getInfinity ()
{
    double n = 0.;
    return 1./n;
}

double fact (double i)
{
    double n = 1., j;
    i = ABS(round(i));
    for (j=1; j <= i; ) n = n*j, j += 1.;
    if (n == getInfinity()) // если слишком большое значение факториала
    {
        error = true; // глобальная переменная для класса Interpreter
        rprin ("\nОшибка: слишком большое значение факториала !\n");
    }
    return n;
}

int NOD (int x, int y) // Нахождение наибольшего общего делителя
{
    if (x == y)
        return abs(x);

    int count = abs(x) > abs(y) ? abs(x) : abs(y); //выбираем наибольшее из чисел
    for (; count > 1; --count)
        if ((!((x) % count)) && (!((y) % count))) // оба числа делятся на счетчик без остатка?
            return count;

    return 0; // НОД не найден
}
double round (double x)
{
    if (fabs(x) - fabs(int(x)) >= 0.5)
    {
        return x > 0. ? 1+(int)x : (int)x-1;
    }
    else
        return int(x);
}

// Функция завершает работу приложения выводя на экран message - собщение об ошибке
void FatalError (const char *message)
{
    rprin ("\n\tИзвините, в ходе работы приложения произошла серьезная ошибка.\n\
\tПричина отказа: ");
    rputs (message);
    printf ("Press <Enter> to continue...");
    getchar ();
    exit (0);
}

// Функция записывает в файл out комплексное число xy с точностью exact знаков после запятой.
// Если exact равно 0, то программа устанавливает точность по своему усмотрению.
// Возвращает количество записаных байтов.
int WriteComplexNumberInFile (double x, double y, FILE *out, int exact)
{
    if (y == 0. && x == 0.)
    {
        return exact ? fprintf (out, "%.*f", exact, 0.) : fprintf (out, "0");
    }

    if (y == 0.)
    {
        return exact ? fprintf (out, "%.*f", exact, x) : fprintf (out, "%g", x);
    }

    if (x == 0.)
    {	if (y == 1.)
            return fprintf (out, "i");
        else
            if (y == -1.)
                return fprintf (out, "-i");
        return exact ? fprintf (out, "%.*fi", exact, y) : fprintf (out, "%gi", y);
    }

    if (y == 1.|| y == -1.)
        return exact ? fprintf (out, "%.*f%ci", exact, x, (y > 0. ? '+' : '-')) : fprintf (out, "%g%ci", x, (y > 0. ? '+' : '-'));
    return exact ? fprintf (out, "%.*f%c%.*fi", exact, x, (y > 0. ? '+' : '-'), exact, ABS(y)) : fprintf (out, "%g%c%gi", x, (y > 0. ? '+' : '-'), ABS(y));
}

inline char convCyrSymb (char c)
{
    if (c >= -64 && c <= -17)
        return c - 64;

    if (c >= -16 && c <= -1)
        return (c - 16);

    return c;
}

char * convCyrStr (char *s)
{
    char *p = s;
    for (; *p; p++)
        *p = convCyrSymb(*p);
    return s;
}
#ifdef WINDOWS
inline int  rputcstd (int c)
{
    return putc (convCyrSymb(c), stdout);
}
#endif

int convert_utf8_to_windows1251(const char* utf8, char* windows1251, size_t n)
{
    typedef class ConvLetter {
    public:
            char    win1251;
            int             unicode;
            ConvLetter (char ch, int i)
            {
                win1251 = ch; unicode = i;
            }
    } Letter;


    static Letter g_letters[] = {
        ConvLetter(0x82, 0x201A), // SINGLE LOW-9 QUOTATION MARK
             ConvLetter(0x83, 0x0453), // CYRILLIC SMALL LETTER GJE
             ConvLetter(0x84, 0x201E), // DOUBLE LOW-9 QUOTATION MARK
             ConvLetter(0x85, 0x2026), // HORIZONTAL ELLIPSIS
             ConvLetter(0x86, 0x2020), // DAGGER
             ConvLetter(0x87, 0x2021), // DOUBLE DAGGER
             ConvLetter(0x88, 0x20AC), // EURO SIGN
             ConvLetter(0x89, 0x2030), // PER MILLE SIGN
             ConvLetter(0x8A, 0x0409), // CYRILLIC CAPITAL LETTER LJE
             ConvLetter(0x8B, 0x2039), // SINGLE LEFT-POINTING ANGLE QUOTATION MARK
             ConvLetter(0x8C, 0x040A), // CYRILLIC CAPITAL LETTER NJE
             ConvLetter(0x8D, 0x040C), // CYRILLIC CAPITAL LETTER KJE
             ConvLetter(0x8E, 0x040B), // CYRILLIC CAPITAL LETTER TSHE
             ConvLetter(0x8F, 0x040F), // CYRILLIC CAPITAL LETTER DZHE
             ConvLetter(0x90, 0x0452), // CYRILLIC SMALL LETTER DJE
             ConvLetter(0x91, 0x2018), // LEFT SINGLE QUOTATION MARK
             ConvLetter(0x92, 0x2019), // RIGHT SINGLE QUOTATION MARK
             ConvLetter(0x93, 0x201C), // LEFT DOUBLE QUOTATION MARK
             ConvLetter(0x94, 0x201D), // RIGHT DOUBLE QUOTATION MARK
             ConvLetter(0x95, 0x2022), // BULLET
             ConvLetter(0x96, 0x2013), // EN DASH
             ConvLetter(0x97, 0x2014), // EM DASH
             ConvLetter(0x99, 0x2122), // TRADE MARK SIGN
             ConvLetter(0x9A, 0x0459), // CYRILLIC SMALL LETTER LJE
             ConvLetter(0x9B, 0x203A), // SINGLE RIGHT-POINTING ANGLE QUOTATION MARK
             ConvLetter(0x9C, 0x045A), // CYRILLIC SMALL LETTER NJE
             ConvLetter(0x9D, 0x045C), // CYRILLIC SMALL LETTER KJE
             ConvLetter(0x9E, 0x045B), // CYRILLIC SMALL LETTER TSHE
             ConvLetter(0x9F, 0x045F), // CYRILLIC SMALL LETTER DZHE
             ConvLetter(0xA0, 0x00A0), // NO-BREAK SPACE
             ConvLetter(0xA1, 0x040E), // CYRILLIC CAPITAL LETTER SHORT U
             ConvLetter(0xA2, 0x045E), // CYRILLIC SMALL LETTER SHORT U
             ConvLetter(0xA3, 0x0408), // CYRILLIC CAPITAL LETTER JE
             ConvLetter(0xA4, 0x00A4), // CURRENCY SIGN
             ConvLetter(0xA5, 0x0490), // CYRILLIC CAPITAL LETTER GHE WITH UPTURN
             ConvLetter(0xA6, 0x00A6), // BROKEN BAR
             ConvLetter(0xA7, 0x00A7), // SECTION SIGN
             ConvLetter(0xA8, 0x0401), // CYRILLIC CAPITAL LETTER IO
             ConvLetter(0xA9, 0x00A9), // COPYRIGHT SIGN
             ConvLetter(0xAA, 0x0404), // CYRILLIC CAPITAL LETTER UKRAINIAN IE
             ConvLetter(0xAB, 0x00AB), // LEFT-POINTING DOUBLE ANGLE QUOTATION MARK
             ConvLetter(0xAC, 0x00AC), // NOT SIGN
             ConvLetter(0xAD, 0x00AD), // SOFT HYPHEN
             ConvLetter(0xAE, 0x00AE), // REGISTERED SIGN
             ConvLetter(0xAF, 0x0407), // CYRILLIC CAPITAL LETTER YI
             ConvLetter(0xB0, 0x00B0), // DEGREE SIGN
             ConvLetter(0xB1, 0x00B1), // PLUS-MINUS SIGN
             ConvLetter(0xB2, 0x0406), // CYRILLIC CAPITAL LETTER BYELORUSSIAN-UKRAINIAN I
             ConvLetter(0xB3, 0x0456), // CYRILLIC SMALL LETTER BYELORUSSIAN-UKRAINIAN I
             ConvLetter(0xB4, 0x0491), // CYRILLIC SMALL LETTER GHE WITH UPTURN
             ConvLetter(0xB5, 0x00B5), // MICRO SIGN
             ConvLetter(0xB6, 0x00B6), // PILCROW SIGN
             ConvLetter(0xB7, 0x00B7), // MIDDLE DOT
             ConvLetter(0xB8, 0x0451), // CYRILLIC SMALL LETTER IO
             ConvLetter(0xB9, 0x2116), // NUMERO SIGN
             ConvLetter(0xBA, 0x0454), // CYRILLIC SMALL LETTER UKRAINIAN IE
             ConvLetter(0xBB, 0x00BB), // RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
             ConvLetter(0xBC, 0x0458), // CYRILLIC SMALL LETTER JE
             ConvLetter(0xBD, 0x0405), // CYRILLIC CAPITAL LETTER DZE
             ConvLetter(0xBE, 0x0455), // CYRILLIC SMALL LETTER DZE
             ConvLetter(0xBF, 0x0457) // CYRILLIC SMALL LETTER YI

    };
    int i = 0;
    int j = 0;
    for(; i < (int)n && utf8[i] != 0; ++i)
    {
        char prefix = utf8[i];
        char suffix = utf8[i+1];
        if ((prefix & 0x80) == 0)
        {
            windows1251[j] = (char)prefix;
            ++j;
        }
        else
            if ((~prefix) & 0x20)
            {
                int first5bit = prefix & 0x1F;
                first5bit <<= 6;
                int sec6bit = suffix & 0x3F;
                int unicode_char = first5bit + sec6bit;


                if ( unicode_char >= 0x410 && unicode_char <= 0x44F )
                {
                    windows1251[j] = (char)(unicode_char - 0x350);
                }
                else
                    if (unicode_char >= 0x80 && unicode_char <= 0xFF)
                    {
                        windows1251[j] = (char)(unicode_char);
                    }
                    else
                        if (unicode_char >= 0x402 && unicode_char <= 0x403) {
                            windows1251[j] = (char)(unicode_char - 0x382);
                        }
                        else
                        {
                            int count = sizeof(g_letters) / sizeof(Letter);
                            for (int k = 0; k < count; ++k)
                            {
                                if (unicode_char == g_letters[k].unicode)
                                {
                                    windows1251[j] = g_letters[k].win1251;
                                    goto NEXT_LETTER;
                                }
                            }
                            // can't convert this char
                            return 0;
                        }
NEXT_LETTER:
                ++i;
                ++j;
            }
            else
            {
                // can't convert this chars
                return 0;
            }
    }
    windows1251[j] = 0;
    return 1;
}

char * get_windows1251(const char * const utf8)
{
    const int len = strlen(utf8);
    char *output = new char[len];
    if (len) output[len-1] = '\0';
    convert_utf8_to_windows1251(utf8, output, len); // перевести строку из кодировки UTF8 в С1251
    return output;
}


// Выводит на консоль строку с русским текстом:
int rprin (const char * s)
{
#ifdef WINDOWS
    char *output = get_windows1251(s);
    const char *p = output;
    for (; *p; rputcstd (*(p++)));
    delete output;
    return p-output;
#else
    return printf(s);
#endif
}

int rputs (const char * const s)
{
    const size_t cSize = rprin (s);
    putchar('\n');
    return 1+cSize;
}

// возвращает истину, если файл с указанным именем в текущем каталоге существует
bool EXISTS (const char *name)
{
    FILE *f;
#ifdef WINDOWS
    char *output = get_windows1251(name);
    bool r = (f = fopen (output, "rt"));
    delete output;
#else
    bool r = (f = fopen (name, "rt"));
#endif
    if (r)
        fclose(f);
    return r;
}

FILE* fopen_ (const char * name, const char * mode)
{
#ifdef LINUX
    return fopen (name, mode);
#else
    char *output = get_windows1251(name);
    char *output2 = get_windows1251(mode);
    FILE* r = fopen ((output), output2);
    delete output;
    delete output2;
    return r;
#endif
}

int isalpha_ (int c)
{
    return isalpha (c) || c >= -128+64 && c <= -81+64 || c >= -32+16 && c <= -17+16;
}

#ifdef LINUX
void convert_cp1251_to_utf8(char *out, const char *in)
{
    static const int table[128] = {
        0x82D0,0x83D0,0x9A80E2,0x93D1,0x9E80E2,0xA680E2,0xA080E2,0xA180E2,
        0xAC82E2,0xB080E2,0x89D0,0xB980E2,0x8AD0,0x8CD0,0x8BD0,0x8FD0,
        0x92D1,0x9880E2,0x9980E2,0x9C80E2,0x9D80E2,0xA280E2,0x9380E2,0x9480E2,
        0,0xA284E2,0x99D1,0xBA80E2,0x9AD1,0x9CD1,0x9BD1,0x9FD1,
        0xA0C2,0x8ED0,0x9ED1,0x88D0,0xA4C2,0x90D2,0xA6C2,0xA7C2,
        0x81D0,0xA9C2,0x84D0,0xABC2,0xACC2,0xADC2,0xAEC2,0x87D0,
        0xB0C2,0xB1C2,0x86D0,0x96D1,0x91D2,0xB5C2,0xB6C2,0xB7C2,
        0x91D1,0x9684E2,0x94D1,0xBBC2,0x98D1,0x85D0,0x95D1,0x97D1,
        0x90D0,0x91D0,0x92D0,0x93D0,0x94D0,0x95D0,0x96D0,0x97D0,
        0x98D0,0x99D0,0x9AD0,0x9BD0,0x9CD0,0x9DD0,0x9ED0,0x9FD0,
        0xA0D0,0xA1D0,0xA2D0,0xA3D0,0xA4D0,0xA5D0,0xA6D0,0xA7D0,
        0xA8D0,0xA9D0,0xAAD0,0xABD0,0xACD0,0xADD0,0xAED0,0xAFD0,
        0xB0D0,0xB1D0,0xB2D0,0xB3D0,0xB4D0,0xB5D0,0xB6D0,0xB7D0,
        0xB8D0,0xB9D0,0xBAD0,0xBBD0,0xBCD0,0xBDD0,0xBED0,0xBFD0,
        0x80D1,0x81D1,0x82D1,0x83D1,0x84D1,0x85D1,0x86D1,0x87D1,
        0x88D1,0x89D1,0x8AD1,0x8BD1,0x8CD1,0x8DD1,0x8ED1,0x8FD1
    };
    while (*in)
        if (*in & 0x80)
        {
            int v = table[(int)(0x7f & *in++)];
            if (!v)
                continue;
            *out++ = (char)v;
            *out++ = (char)(v >> 8);
            if (v >>= 16)
                *out++ = (char)v;
        }
        else
            *out++ = *in++;
    *out = 0;
}

char * get_utf8(const char * const cp1251)
{
    const int len = strlen(cp1251) * 7; // один символ utf8 занимает до 6 байт, примем с запасом
    char *output = new char[len];
    if (len) output[len-1] = '\0';
    convert_cp1251_to_utf8(output, cp1251); // перевести строку из кодировки СP1251 в UTF8
    return output;
}
#endif
