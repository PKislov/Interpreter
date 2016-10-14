#ifndef COMPLEX_H
#define COMPLEX_H

/////////////////////////////////////////////////////////////////////////////////////////////////////////
///     complex.h - описание класса complex для работы с комплексными числами.
///
///                     ИНТЕРПРЕТАТОР 1.0
///
/////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "definitions.h"

// Объекты типа complex возвращаются главным методом interpret класса Interpreter.
// Ниже описанные методы используются для вычисления основных функций калькулятора.
struct complex
{
    double Re, Im; // Действительная и комплексная часть числа

    // умножение комплексных чисел temp1 и temp2, результат поместить в result
    void multiplication (complex &temp1, complex &temp2, complex *result)
    {
        result->Re = temp1.Re * temp2.Re - temp1.Im * temp2.Im;
        result->Im = temp1.Re * temp2.Im + temp1.Im * temp2.Re;
    }
//=============================================================
    // деление комплексных чисел temp1 и temp2, результат поместить в result
    void division (complex &temp1, complex &temp2, complex *result)
    {
        double de = temp2.Re * temp2.Re + temp2.Im * temp2.Im;
        if (de == 0.)
        {
            rprin ("Вы попытались поделить на 0 !");
            error = true;
        }
        result->Re = (temp1.Re * temp2.Re + temp1.Im * temp2.Im) / de;
        result->Im = (temp1.Im * temp2.Re - temp1.Re * temp2.Im) / de;
    }
//=============================================================
    // возвращает модуль комплексного числа
    double module (complex &temp)
    {
        return sqrt (temp.Re * temp.Re + temp.Im * temp.Im);
    }
//=============================================================
    // найти модуль комплексного числа, результат поместить в temp
    void complex_module (complex *temp)
    {
        temp->Re = module (*temp);
        temp->Im = 0.;
    }
//=============================================================
    // в temp обнуляет действительную часть числа
    void getre_ (complex *temp)
    {
        temp->Im = 0.;
    }
//=============================================================
    // в temp обнуляет комплексную часть числа
    void getim_ (complex *temp)
    {
        temp->Re = 0.;
    }
//=============================================================
    // найти аргумент комплексного числа (в градусах), результат поместить в temp
    void complex_arg (complex *temp)
    {
        temp->Re = arg (*temp);
        temp->Im = 0.;
    }
//=============================================================
    // возвращает аргумент комплексного числа в градусах
    double arg (complex &temp)
    {
        if (temp.Re == 0. && temp.Im == 0.)
        {
            rprin ("Arg (0) - величина неопределенная!");
            error = true;
            return 0;
        }

        if (temp.Re > 0.)
            return ToGrad (atan (temp.Im / temp.Re));
        else
            if (temp.Re < 0. && temp.Im >= 0.)
                return 180. + ToGrad (atan (temp.Im / temp.Re));
            else
                if (temp.Re < 0. && temp.Im < 0.)
                    return -180. + ToGrad (atan (temp.Im / temp.Re));
                else
                    if (temp.Re == 0. && temp.Im > 0.)
                        return 90.;
                    else
                        if (temp.Re == 0. && temp.Im < 0.)
                            return -90.;
    }
//=============================================================
    // экспонента, результат записывается в temp
    void exp_ (complex *temp)
    {
        double ex = exp(temp->Re), im = temp->Im;
        temp->Im = ex * sin (temp->Im);
        temp->Re = ex * cos(im);
    }
//=============================================================
    // натуральный логарифм числа, результат записывается в temp
    void ln_ (complex *temp)
    {
        double Re = log (module (*temp));
        if (temp->Im != 0. || temp->Re <= 0.)
            temp->Im = ToRadians(arg (*temp));
        temp->Re = Re;
    }
//=============================================================
    // в result записывается значение temp1 в степени temp2
    void pow_ (complex &temp1, complex &temp2, complex *result)
    {
        if (temp1.Im == 0. && (temp2.Re >= 1. || temp2.Re <= -1.) && temp2.Im == 0.)
        {
            result->Im = 0.;
            result->Re = pow (temp1.Re, temp2.Re);
            if (temp1.Re < 0. && temp2.Re < 0.)
                result->Re = -result->Re;
            return;
        }
        if (temp1.Re == 0. &&
            temp1.Im == 0. &&
            (temp2.Re != 0. || temp2.Im != 0.)) // ноль в степени...
        {
            result->Re = result->Im = 0.;
            return;
        }
        else
            if (temp1.Re == 0. &&
                temp1.Im == 0. &&
                temp2.Re == 0. &&
                temp2.Im == 0.) // ноль в степени ноль
            {
                result->Re = 1.;
                result->Im = 0.;
                return;
            }
        result->Re = result->Im = 0.;
        // z^a = exp(a*ln(z)), а - комплексная степень
        complex a = temp2, Ln_z = temp1;
        ln_ (&Ln_z);
        multiplication (a, Ln_z, result);
        exp_ (result);
    }
//=============================================================
    // синус числа temp, результат записывается в temp. Все остальные методы - аналогично.
    void sin_ (complex *temp)
    {
        if (temp->Im == 0.)
        {
            temp->Re = sin (ToRadians (temp->Re));
        }
        else
        {
            complex e1, e2;
            e1.Re = -temp->Im;
            e1.Im = temp->Re;
            e2.Re = temp->Im;
            e2.Im = -temp->Re;
            exp_ (&e1);
            exp_ (&e2);
            e1.Re -= e2.Re;
            e1.Im -= e2.Im;
            e2.Re = 0.;
            e2.Im = 2.;
            division (e1, e2, temp);
        }
    }
//=============================================================
    // косинус
    void cos_ (complex *temp)
    {
        if (temp->Im == 0.)
        {
            temp->Re = cos (ToRadians (temp->Re));
        }
        else
        {
            complex e1, e2;
            e1.Re = -temp->Im;
            e1.Im = temp->Re;
            e2.Re = temp->Im;
            e2.Im = -temp->Re;
            exp_ (&e1);
            exp_ (&e2);
            temp->Re = (e1.Re + e2.Re) / 2.;
            temp->Im = (e1.Im + e2.Im) / 2.;
        }
    }
//=============================================================
    // тангенс
    void tg_ (complex *temp)
    {
        if (temp->Im == 0.)
        {
            temp->Re = tan (ToRadians (temp->Re));
        }
        else
        {
            complex e1 = *temp, e2 = *temp;
            sin_ (&e1);
            cos_ (&e2);
            division (e1, e2, temp);
        }
    }
//=============================================================
    // котангенс
    void ctg_ (complex *temp)
    {
        if (temp->Im == 0.)
        {
            temp->Re = 1./ tan (ToRadians (temp->Re));
        }
        else
        {
            complex e1 = *temp, e2 = *temp;
            sin_ (&e1);
            cos_ (&e2);
            division (e2, e1, temp);
        }
    }
//=============================================================
    // арксинус
    void asin_ (complex *temp)
    {
        if ((temp->Re <= 1.  && temp->Im >= -1.)&& temp->Im == 0.)
        {
            temp->Re = ToGrad(asin (temp->Re)); // результат в градусах
            return;
        }
            complex z = *temp, z2 = *temp, box;
            box.Re = 0.; box.Im = 1.;

            sqr_ (&z);
            z.Im *= -1.;
            z.Re = 1. - z.Re;
            sqrt_ (&z);
            multiplication (z2, box, temp);
            z2 = *temp;
            z.Re += z2.Re;
            z.Im += z2.Im;
            ln_ (&z);
            division (z, box, temp);
    }
//=============================================================
    // арккосинус
    void acos_ (complex *temp)
    {
        if ((temp->Re <= 1.  && temp->Im >= -1.)&& temp->Im == 0.)
        {
            temp->Re = ToGrad(acos (temp->Re));
            return;
        }
            complex z = *temp, box;
            box.Re = 0.; box.Im = -1.;
            sqr_ (&z);
            z.Re -= 1.;
            sqrt_ (&z);
            z.Re += temp->Re;
            z.Im += temp->Im;
            ln_ (&z);
            multiplication(z, box, temp);
            if (temp->Re < 0. && temp->Im < 0.)
            {
                temp->Re = -temp->Re;
                temp->Im = -temp->Im;
            }
    }
//=============================================================
    // квадрат числа
    void sqr_ (complex *temp)
    {
        double Re_ = temp->Re, Im_ = temp->Im;
        temp->Re = Re_*Re_ - Im_*Im_;
        temp->Im = 2.*Re_*Im_;
    }
//=============================================================
    // квадратный корень числа
    void sqrt_ (complex *temp)
    {
        complex z1 = *temp, pow_z = {0.5, 0.};
        pow_ (z1, pow_z, temp);
    }
//=============================================================
    // арктангенс
    void atg_ (complex *temp)
    {
        if (temp->Im == 0.)
        {
            temp->Re = ToGrad(atan (temp->Re)); temp->Im = 0.;
            return;
        }
        complex z1, z2, box = {0, -0.5};
        z1 = z2 = *temp;
        z1.Re = -z1.Re;
        z1.Im = -z1.Im;
        z1.Im += 1.;
        z2.Im += 1.;
        division (z1, z2, temp);
        z1 = *temp;
        ln_ (&z1);
        multiplication (z1, box, temp);
    }
//=============================================================
    // арккотангенс
    void actg_ (complex *temp)
    {
        if (temp->Im == 0.)
        {
            temp->Re = 90. - ToGrad(atan (temp->Re));
            return;
        }
        complex z1, z2, box = {0, 0.5};
        z1 = z2 = *temp;
        z1.Im -= 1.;
        z2.Im += 1.;
        division (z1, z2, temp);
        z1 = *temp;
        ln_ (&z1);
        multiplication (z1, box, temp);
    }
//=============================================================
    // десятичный логарифм
    void log10_ (complex *temp)
    {
        complex z1 = *temp, z2 = {log (10), 0};
        ln_ (&z1);
        division (z1, z2, temp);
    }
//=============================================================
    // кубический корень числа
    void cbrt_ (complex *temp)
    {
        complex z1 = *temp, pow_z = {1./3., 0.};
        pow_ (z1, pow_z, temp);
    }
//=============================================================
    // куб числа
    void cub_ (complex *temp)
    {
        complex z1 = *temp, pow_z = {3., 0.};
        pow_ (z1, pow_z, temp);
    }
//=============================================================
    // округление действительной части числа до ближайшего целого
    void round_ (complex *temp)
    {
        temp->Re = round (temp->Re);
        //temp->Im = round (temp->Im);
    }
//=============================================================
    // расчет гипотенузы по двум катетам temp1 и temp2, только числа комплексные.
    // Результат запишется в temp1. Остальные методы - аналогично.
    void hypot_ (complex *temp1, complex *temp2)
    {
        sqr_ (temp1);
        sqr_ (temp2);
        temp1->Re += temp2->Re;
        temp1->Im += temp2->Im;
        sqrt_ (temp1);
    }
//=============================================================
    // логарифм temp1 по основанию temp2
    void log_ (complex *temp1, complex *temp2)
    {
        complex z1 = *temp1, z2 = *temp2;
        ln_ (&z1);
        ln_ (&z2);
        division (z1, z2, temp1);
    }
//=============================================================
    // гиперболический синус
    void sh_ (complex *temp)
    {
            complex e1 = *temp, e2 = *temp;
            e2.Re = -e2.Re;
            e2.Im = -e2.Im;
            exp_ (&e1);
            exp_ (&e2);
            e1.Re -= e2.Re;
            e1.Im -= e2.Im;
            e2.Re = 2.;
            e2.Im = 0.;
            division (e1, e2, temp);
    }
//=============================================================
    // гиперболический косинус
    void ch_ (complex *temp)
    {
            complex e1 = *temp, e2 = *temp;
            e2.Re = -e2.Re;
            e2.Im = -e2.Im;
            exp_ (&e1);
            exp_ (&e2);
            e1.Re += e2.Re;
            e1.Im += e2.Im;
            e2.Re = 2.;
            e2.Im = 0.;
            division (e1, e2, temp);
    }
//=============================================================
    // гиперболический тангенс
    void th_ (complex *temp)
    {
            complex e1 = *temp, e2 = *temp;
            sh_ (&e1);
            ch_ (&e2);
            division (e1, e2, temp);
    }
//=============================================================
    // гиперболический котангенс
    void cth_ (complex *temp)
    {
            complex e1 = *temp, e2 = *temp;
            sh_ (&e1);
            ch_ (&e2);
            division (e2, e1, temp);
    }
//=============================================================
    // гиперболический арксинус
    void ash_ (complex *temp)
    {
        complex z = *temp;
        sqr_ (&z);
        z.Re += 1.;
        sqrt_ (&z);
        z.Re += temp->Re;
        z.Im += temp->Im;
        ln_ (&z);
        temp->Re = z.Re;
        temp->Im = z.Im;
    }
//=============================================================
    // гиперболический арккосинус
    void ach_ (complex *temp)
    {
        complex z = *temp;
        sqr_ (&z);
        z.Re -= 1.;
        sqrt_ (&z);
        z.Re += temp->Re;
        z.Im += temp->Im;
        ln_ (&z);
        temp->Re = z.Re;
        temp->Im = z.Im;
    }
//=============================================================
    // гиперболический арктангенс
    void ath_ (complex *temp)
    {
        complex z1, z2, box = {0.5, 0.};
        z1 = z2 = *temp;
        z2.Re = -z1.Re;
        z2.Im = -z1.Im;
        z1.Re += 1.;
        z2.Re += 1.;
        division (z1, z2, temp);
        z1 = *temp;
        ln_ (&z1);
        multiplication (z1, box, temp);
    }
//=============================================================
    // гиперболический арккотангенс
    void acth_ (complex *temp)
    {
        complex z1, z2, box = {0.5, 0.};
        z1 = z2 = *temp;
        z1.Re += 1.;
        z2.Re -= 1.;
        division (z1, z2, temp);
        z1 = *temp;
        ln_ (&z1);
        multiplication (z1, box, temp);
    }
//=============================================================
    // комплексно-сопряженное temp
    void conj_ (complex *temp)
    {
        temp->Im = -temp->Im;
    }
//=============================================================
    // секанс
    void sec_ (complex *temp)
    {
        if (temp->Im == 0.)
        {
            if ((temp->Re = cos (ToRadians (temp->Re))) == 0.)
            {
                rprin ("Вы попытались поделить на 0 !");
                error = true;
            }
            else
            {
                temp->Re = 1. / temp->Re;
            }
        }
        else
        {
            complex e1, e2;
            e1.Re = -temp->Im;
            e1.Im = temp->Re;
            e2.Re = temp->Im;
            e2.Im = -temp->Re;
            exp_ (&e1);
            exp_ (&e2);
            temp->Re = (e1.Re + e2.Re) / 2.;
            temp->Im = (e1.Im + e2.Im) / 2.;
            e1.Re = 1.;
            e1.Im = 0.;
            division (e1, *temp, &e2);
            temp->Re = e2.Re;
            temp->Im = e2.Im;
        }
    }
//=============================================================
    // косеканс
    void csec_ (complex *temp)
    {
        if (temp->Im == 0.)
        {
            if ((temp->Re = sin (ToRadians (temp->Re))) == 0.)
            {
                rprin ("Вы попытались поделить на 0 !");
                error = true;
            }
            else
            {
                temp->Re = 1. / temp->Re;
            }
        }
        else
        {
            complex e1, e2;
            e1.Re = -temp->Im;
            e1.Im = temp->Re;
            e2.Re = temp->Im;
            e2.Im = -temp->Re;
            exp_ (&e1);
            exp_ (&e2);
            e1.Re -= e2.Re;
            e1.Im -= e2.Im;
            e2.Re = 0.;
            e2.Im = 2.;
            division (e1, e2, temp);
            e1.Re = 1.;
            e1.Im = 0.;
            division (e1, *temp, &e2);
            temp->Re = e2.Re;
            temp->Im = e2.Im;
        }
    }
//=============================================================
    // перевести градусы в радианы
    void torad_ (complex *temp)
    {
        temp->Re *= (M_PI / 180.0);
        temp->Im *= (M_PI / 180.0);
    }
//=============================================================
    // перевести радианы в градусы
    void todeg_ (complex *temp)
    {
        temp->Re *= (180.0 / M_PI);
        temp->Im *= (180.0 / M_PI);
    }

};

#endif // COMPLEX_H

