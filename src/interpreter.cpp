
/////////////////////////////////////////////////////////////////////////////////////////////////////////
///     Методы класса Interpreter
///
///                     ИНТЕРПРЕТАТОР 1.0
/////////////////////////////////////////////////////////////////////////////////////////////////////////


#include "interpreter.h"
#include "definitions.h"
#include "complex.h"

extern char ListNamesUnique [][15];


extern struct complex answer, // предыдущее вычисленное значение, определяет метку ans в
                               // выражении, изначально равно 0

               temp_complex_number, // используется методами Interpreter при вычислении
                                    // функций и операторов

               null, // возвращается методом interpret, если в выражении нашлась ошибка

               return_value, // возвращается методом interpret, если выполнена функция return
               resultCalcul; //  результат вычисления выражения


extern char **strings;      // массив строк, заполняется в ходе выполнения выражения,
// а после удаляется

extern FILE *formula, // файл с кодом программы
     *result;  // файл для записи данных функциями write и writeln.

extern int exact; // определяет число знаков после запятой, с которым отображаются числа.

extern struct Variable // структура пользовательской переменной
        *ListVar; // массив пользовательских переменных

extern int count;  // счетчик. Используется при создании массивов.

extern struct UserArray *ListArray; // структура пользовательского массива

extern struct Function // структура пользовательской функции
 *ListFunctions; // Массив пользовательских функций

extern int count_recurs;   // счетчик рекурсивных вызовов пользовательской функции,
// необходим для реализации механизма рекурсии.

extern void DeleteStrings (int countStr);



//=================== ОПИСАНИЕ МЕТОДОВ ГЛАВНОГО КЛАССА ==========================================================

// В параметрах передается строка содержащая выражение.
// Возвращает комплексное число - результат вычисления.
complex Interpreter::interpret (char *str)
{
    p = str; // инициализировать глобальный указатель p

    if (!*str) // если в функцию передано пустое выражение
    {
        ThisComment = true; // флаг означает, что пустое выражение
        return null; // возвратить нулевое комплексное число
    }

    // исходное состояние флагов: непустое выражение, ошибки не найдены
    ThisComment = error = false;

    FillingListLexeme (str);    // создать список лексем    

    if (error) // если в ходе построения списка найдены ошибки
    {
        Undo (); // удалить список лексем
        return null; // возвратить нулевое комплексное число
    }

    if (countToken == 0) // если список лексем пустой
    {
        ThisComment = true; // флаг означает, что пустое выражение
        return null; // возвратить нулевое комплексное число
    }

    SearchErrors (); // функция поиска некоторых синтаксических ошибок в списке лексем
    if (error) // если найдены синтаксические ошибки
    {
        Undo (); // удалить список лексем
        return null; // возвратить нулевое комплексное число
    }

    complex a, b, c; // эти переменные используются для вычисления некоторых операторов

    // в данном цикле производится замена всех обращений к действительной или
    // комплексной части переменных на функции getre или getim, т.е. если var равно 5+2i, то
    // утверждение var.re+2 примет вид getre(var)+2, в случае инициализации замена не
    // производится: va.re=3
    for (temp = start; temp; temp = temp->next) // прочитать весь список лексем
    {        
        // если текущая лексема - оператор точка
        if (temp->ThisIs == POINT)
        {
            // и если слева и справа от точки есть по лексеме
            if (temp->prior && temp->next)
            {
                // если лексема слева от точки - не имя переменной или справа не квалификатор
                // re или im
                if (temp->prior->ThisIs != VARIABLE ||
                    !IsMemberStruct (temp->next))
                {
                    break;
                }

                // если справа от точки есть две лексемы
                if (temp->next->next)
                {
                    // если вторая лексема - оператор присваивания
                    if (temp->next->next->ThisIs == EQUAL)
                    {
                        continue; // не производить замены
                    }
                }

                // сделать замену, например var.re (3 лексемы) -> getre(var) (4 лексемы).
                // Новую лексему вставим в список между точкой и квалификатором re и
                // скопируем в неё переменную, лексему var определить как getre,
                // точку как отрывающуюся скобку, квалификатор как закрывающуюся скобку

                temp = temp->prior; // temp теперь указывает на имя переменной
                token *box; // новая лексема
                box = new token;
                if (!box)
                    FatalError ("Ошибка выделения памяти");
                *box = *temp; // в box скопировать переменную
                ++countToken; // одной лексемой в списке стало больше
                // переопределение связей в списке лексем - вставка box
                box->prior = temp->next;
                box->next = temp->next->next;
                temp->next->next = box;
                box->next->prior = box;

                // в зависимости от квалификатора, определить getre или getim
                if (box->next->ThisIs == MEMBER_STRUCT_RE)
                {
                    temp->ThisIs = GETRE;
                }
                else
                {
                    temp->ThisIs = GETIM;
                }
                // определить скобки вокруг аргумента функции
                temp->next->ThisIs = BRECKET_OPEN;
                box->next->ThisIs = BRECKET_CLOSE;
                temp = temp->next->next->next; // temp установить на закрывающуюся скобку
            } // if (temp->prior && temp->next)
            else
            {
                goto error_; // если точка - первая или последняя лексема в списке,
                // перейти к действиям обработки ошибки
            }
        } // if (temp->ThisIs == POINT)
    } // for (temp = start; temp; temp = temp->next) замена всех обращений к действительной...


    // Цикл вычисления выражения. При циклическом просмотре списка лексем
    // происходит последовательное упрощение выражения путем удаления лишних скобок или
    // заменой ряда совокупностей чисел и операторов/функций на результат их вычисления
    // при строгом соблюдении приоритета операторов, таким образом список лексем шаг за
    // шагом становится все короче, при отсутствии синтаксических ошибок в списке лексем
    // должен остаться один элемент - число, конечный результат выражения, т.е. список
    // вырождается в одну лексему - число. При наличие синтаксических ошибок при
    // достижении определенного числа итераций цикла выводится сообщение об ошибке,
    // удаляется список лексем из памяти, глобальная переменная-флаг error устанавливается
    // в true и метод прекращает свою работу. То же самое происходит и при обнаружении
    // чисто математических ошибок типа 3/0 или tg 90.
    for (iteration = 0; ; ++iteration) // цикл вычисления значения выражения
    {
anew:   temp = start; // переход к началу списка лексем        

        // условие обнаружения синтаксических ошибок. Ошибка возникает, когда
        // программа зацикливается
        if (iteration == LIMITE_ERROR)
        {
error_:     rprin ("Синтаксическая ошибка!");
            Undo (); // удалить список лексем
            // флаг - глобальная переменная - показывает, что в выражении ошибка
            error = true;
            return null; // возвратить нулевое комплексное число
        }

        while (temp) // цикл просмотра списка
        {
loop:;
           // debug ();
// --- Выполнение команды exit
            if (temp->ThisIs == EXIT)
            {
                exit (0);
            }

// --- Выполнение команды break
            // если текущае лексема - оператор break или в каком-нибудь блоке фигурных
            // скобок была выполнена команда break
            if (temp->ThisIs == BREAK || run_break)
            {
                // если команда break втретилась вне цикла, т.е. счетчик
                // вложенности циклов равен 0
                if (!count_loop)
                {
                    rprin ("Неуместное использование команды break");
                    Undo (); // удалить список лексем
                    error = true; // флаг - глобальная переменная - показывает, что в выражении ошибка
                }
                DeleteAll(&start); // удалить список лексем
                run_break = true; // флаг означает что команда выполнилась
                return null; // возвратить нулевое комплексное число

            }

// --- Выполнение команды continue
            // если текущае лексема - оператор continue или в каком-нибудь блоке фигурных
            // скобок была выполнена команда continue
            if (temp->ThisIs == CONTINUE || run_continue)
            {
                // если команда continue втретилась вне цикла, т.е. счетчик
                // вложенности циклов равен 0
                if (!count_loop)
                {
                    rprin ("Неуместное использование команды continue");
                    Undo (); // удалить список лесем
                    error = true; // флаг - глобальная переменная - показывает, что в выражении ошибка
                }
                run_continue = true; // флаг означает что команда выполнилась
                DeleteAll(&start); // удалить список лесем
                return null; // возвратить нулевое комплексное число
            }

            // условие окончания цикла вычисления, когда в списке осталась одна лексема
            if (countToken == 1)
            {
                // если эта лексема не число и не переменная и не логическое значение и
                // не идентификатор cout
                if (!IsNumberOrVariable(temp) && temp->ThisIs != COUT1)
                {
                    goto error_; // перейти к обработке ошибки
                }
                resultCalcul.Re = start->number; // в переменную result записать ответ
                resultCalcul.Im = start->Im_number;
                delete start; // удалить последнюю лексему
                return_func = false; // флаг показывает, что функция return не выполнялась в выражении
                return resultCalcul; // возвратить результат вычисления - комплексное число
            }

// --- Объявление и иициализация переменных

        // перечисление переменных a,d,b=45,h;
        if (!temp->prior &&
            temp->next &&
            IsNumberOrVariable(temp)) // если текущая лексема - первая в списке и справа
            // есть лексема и текущая лексема - число или переменная
        {
            // если лексема спрва - запятая
            if (temp->next->ThisIs == COMMA)
            {
                DelElementMyVersion (temp->next, &start, &end); // удалить запятую
                DelElementMyVersion (temp, &start, &end); // удалить текущую лексему
                countToken -= 2; // на две лексемы в списке стало меньше
                goto anew; // переход к началу списка лексем
            }
        }        

        // множественное или одиночное присваивание, например a=b=v=5 или a=5;
        if (temp->next &&
            temp->next->next &&
            (!temp->next->next->next)) // если справа от текущей лексемы есть только две лексемы
        {
            if (temp->ThisIs == VARIABLE &&
                temp->next->ThisIs == EQUAL &&
                IsNumberOrVariable (temp->next->next)) // если текущая лексема - переменная,
                // следующая лексема - оператор присваивания, вторая лексема - число или
                // переменная
            {
                // сохранить новое значение переменной
                SaveValue (temp->next->next->number, temp->next->next->Im_number);
                Del_2_Elements (); // удалить из списка две следующие лексемы

                // если слева нет лексем, т.е. в списке осталась одна лексема
                if (!temp->prior)
                {
                    delete start; // удалить последнюю лексему из списка
                    ThisComment = true; // у выражения нет числового ответа
                    return null; // возвратить нулевое комплексное число
                }
                goto anew; // переход к началу списка лексем
            }
        }

        // присваивание в cкобках (а=5), в т.ч. множественное
        if (temp->next &&
            temp->next->next &&
            temp->next->next->next) // если справа от текущей лексемы есть 3 лексемы
        {
            if (temp->ThisIs == VARIABLE && // текущая лексема - переменная
                temp->next->ThisIs == EQUAL && // следующая - оператор присваивания
                IsNumberOrVariable (temp->next->next) && // вторая - число или переменная
                temp->next->next->next->ThisIs == BRECKET_CLOSE) // третья - закрывающаяся скобка
            {
                // сохранить новое значение переменной
                SaveValue (temp->next->next->number, temp->next->next->Im_number);
                Del_2_Elements (); // удалить следующие две лексемы
                goto anew; // перейти к началу списка
            }
        }

        // присваивание через запятую a=3, b=a или просто объявление переменных;
        if (temp->next &&
            temp->next->next &&
            temp->next->next->next) // если справа есть 3 лексемы
        {
            // если текущая лексема - переменная и далее оператор присваивания, число или
            // переменная, запятая
            if (temp->ThisIs == VARIABLE &&
                temp->next->ThisIs == EQUAL &&
                IsNumberOrVariable (temp->next->next) &&
                temp->next->next->next->ThisIs == COMMA)
            {
                // сохранить новое значение переменной
                SaveValue (temp->next->next->number, temp->next->next->Im_number);
                Del_2_Elements (); // удалить оператор присваивания и число
                goto anew; // перейти к началу списка
            }
        }

// --- Блок инициализации комплексной или действительной части переменной

        // множественное или одиночное присваивание, например a.re=b.re=v.re=5 или a.re=5;
        if (temp->next &&
            temp->next->next &&
            temp->next->next->next &&
            temp->next->next->next->next &&
            (!temp->next->next->next->next->next)) // если следующие 4 лексемы - последние в списке
            if (temp->ThisIs == VARIABLE && // если текущая лексема - переменная и далее точка, квалификатор re или im, оператор присваивания, число или переменная
                temp->next->ThisIs == POINT &&
                IsMemberStruct (temp->next->next) &&
                temp->next->next->next->ThisIs == EQUAL &&
                IsNumberOrVariable (temp->next->next->next->next))
            {
                // если вторая лексема - квалификатор re
                if (temp->next->next->ThisIs == MEMBER_STRUCT_RE)
                {
                    // сохранить новое значение переменной
                    SaveValue (temp->next->next->next->next->number, temp->Im_number);
                    // в текущую лексему записать вещественную часть числа, которое после оператора присваивания
                    temp->number = temp->next->next->next->next->number;
                    temp->Im_number = 0.;
                }
                else // иначе если квалификатор im
                {
                    SaveValue (temp->number, temp->next->next->next->next->Im_number);
                    temp->Im_number = temp->next->next->next->next->Im_number;
                    temp->number = 0.;
                }
                temp->ThisIs = NUMBER; // на место текущей лексемы записать значение, которым инициализировали
                Del_2_Elements (); // удалить из списка точку, квалификатор, оператор присваивания и число
                Del_2_Elements ();

                if (!temp->prior) // если слева нет лексем, т.е. в списке осталась одна лексема
                {
                    DeleteAll (&start);
                    ThisComment = true; // у выражения нет числового ответа
                    return null; // возвратить нулевое комплексное число
                }
                goto anew; // переход к началу списка лексем
            }

        // присваивание в cкобках (а.re=5), в т.ч. множественное
        if (temp->next &&
            temp->next->next &&
            temp->next->next->next &&
            temp->next->next->next->next &&
            temp->next->next->next->next->next) // если справа есть 5 лексем
        {
            // если текущая лексема - переменная, затем точка, квалификатор re или im,
            // оператор присваивания, число или переменная, закрывающаяся скобка
            if (temp->ThisIs == VARIABLE &&
                temp->next->ThisIs == POINT &&
                IsMemberStruct (temp->next->next) &&
                temp->next->next->next->ThisIs == EQUAL &&
                IsNumberOrVariable (temp->next->next->next->next) &&
                temp->next->next->next->next->next->ThisIs == BRECKET_CLOSE)
            {
                // если вторая лексема - квалификатор re
                if (temp->next->next->ThisIs == MEMBER_STRUCT_RE)
                {
                    // сохранить новое значение переменной
                    SaveValue (temp->next->next->next->next->number, temp->Im_number);
                    temp->number = temp->next->next->next->next->number;
                    temp->Im_number = 0.;
                }
                else // иначе если квалификатор im
                {
                    SaveValue (temp->number, temp->next->next->next->next->Im_number);
                    temp->Im_number = temp->next->next->next->next->Im_number;
                    temp->number = 0.;
                }
                // на место текущей лексемы записать значение, которым инициализировали
                temp->ThisIs = NUMBER;
                Del_2_Elements (); // удалить из списка точку, квалификатор, оператор присваивания и число
                Del_2_Elements ();
                goto anew; // перейти к началу списка
            }
        }

        // присваивание через запятую a.re=3, b.im=a или просто объявление переменных
        if (temp->next &&
            temp->next->next &&
            temp->next->next->next &&
            temp->next->next->next->next &&
            temp->next->next->next->next->next) // если справа есть 5 лексем
        {
            // если текущая лексема - имя переменной, затем точка, квалификатор re или im,
            // оператор присваивания, число или переменная, запятая
            if (temp->ThisIs == VARIABLE &&
                temp->next->ThisIs == POINT &&
                IsMemberStruct (temp->next->next) &&
                temp->next->next->next->ThisIs == EQUAL &&
                IsNumberOrVariable (temp->next->next->next->next) &&
                temp->next->next->next->next->next->ThisIs == COMMA)
            {
                // если вторая лексема - квалификатор re
                if (temp->next->next->ThisIs == MEMBER_STRUCT_RE)
                {
                    // сохранить новое значение переменной
                    SaveValue (temp->next->next->next->next->number, temp->Im_number);
                    temp->number = temp->next->next->next->next->number;
                    temp->Im_number = 0.;
                }
                else // иначе если квалификатор im
                {
                    SaveValue (temp->number, temp->next->next->next->next->Im_number);
                    temp->Im_number = temp->next->next->next->next->Im_number;
                    temp->number = 0.;
                }
                // на место текущей лексемы записать значение, которым инициализировали
                temp->ThisIs = NUMBER;
                Del_2_Elements (); // удалить из списка точку, квалификатор, оператор присваивания и число
                Del_2_Elements ();
                ThisComment = true; // у выражения нет числового ответа
                goto anew; // перейти к началу списка
            }
        }

// --- Блок обработки массивов

        run_new                         (); // создание массива с помощью функции new
        element_array_1D_to_number      (); // перевести элемент одномерного массива в число
        element_array_2D_to_number      (); // перевести элемент двумерного массива в число
        initialization_element_array_1D (); // инициализация элемента одномерного массива
        initialization_element_array_2D (); // инициализация элемента двумерного массива


// --- Блок вычисления унарных операторов

            UnMinus (); // вычислить минус числа: - 2 -> -2
            UnPlus  (); // вычислить плюс числа: +2+3 -> 2+3
            Not     (); // оператор логического отрицания, not 1 -> false

            Factorial               (); // вычислить факториал, 3! -> 6
            DelBrecketsAroundNumber (); // удалить скобки вокруг числа, панример (2) -> 2
            DelModulsAroundNumber   (); // найти модуль числа, например |-2| -> 2

// --- Блок вычисления функций-операторов, sin 30 -> 0.5

            Functions_Operators_Of1Arguments ();
            if (error) // если возникла ошибка
            {
                Undo (); // удалить список лесем
                return null; // возвратить нулевое комплексное число
            }

// --- Блок вычисления функций от одного аргумента, sin(30) -> 0.5

            Functions_Of1Arguments ();
            if (error) // если возникла ошибка
            {
                Undo (); // удалить список лесем
                return null; // возвратить нулевое комплексное число
            }

// --- Блок вычисления функций от двух аргументов.

            Functions_Of2Arguments ();
            if (error) // если возникла ошибка
            {
                Undo (); // удалить список лесем
                return null; // возвратить нулевое комплексное число
            }

// --- Блок вычисления интегралов

            integral ();

            if (error) // если возникла ошибка
            {
                Undo (); // удалить список лесем
                return null; // возвратить нулевое комплексное число
            }

// --- Блок вычисления операторов программирования

            run_if ();          // условие if
            run_dowhile ();     // цикл while
            run_for ();         // цикл for
            run_while ();       // цикл с предусловием do-while
            creatFunction ();   // объявление пользовательских функций - команда func

            if (error) // если возникла ошибка
            {
                Undo (); // удалить список лесем
                return null; // возвратить нулевое комплексное число
            }

            // список лексем может быть пустым в том случае, если была объявлена
            // функция с помощью команды func
            if (!countToken)
            {
                ThisComment = true; // у выражения нет числового ответа
                return null; // возвратить нулевое комплексное число
            }

            // если выполнилась функция return
            if (return_func)
            {
                DeleteAll (&start); // удалить список лексем из памяти
                ThisComment = false;
                return return_value; // возвратить аргумент функции return
            }

// --- Блок вычисления бинарных операторов

            run_cout (); // оператор cout

            // если найдено число или переменная и после числа есть две лексемы
            if (IsNumberOrVariable(temp) &&
                temp->next &&
                temp->next->next)
            {
                // если вторая лексема после числа также число или переменная
                    if (IsNumberOrVariable(temp->next->next))
                    {
                        // узнать, какой оператор между числами
                        switch (temp->next->ThisIs)
                        {
                            case MOD: // остаток по модулю

                                // если оба числа - вещественные
                                if (IsRealNumberOrVar (temp) &&
                                    IsRealNumberOrVar (temp->next->next))
                                {
                                    // если второе число = 0
                                    if (!(int)temp->next->next->number)
                                    {
                                        rprin ("Вы попытались поделить на 0 !");
                                        Undo (); // удалить список лесем
                                        error = true;
                                        return null; // возвратить нулевое комплексное число
                                    }
                                    // найти остаток от деления по модулю
                                    temp->number = ABS (((int)temp->number) % ((int)temp->next->next->number)); // вычислить остаток
                                    // текущую лексему определить как число и записать
                                    // в неё результат
                                    temp->ThisIs = NUMBER;
                                    // удалить две лексемы справа от текущей
                                    Del_2_Elements  ();
                                    goto anew; // перейти к началу списка
                                }

                                // если одно из чисел - комплексное
                                if (IsImNumberOrVar (temp) ||
                                    IsImNumberOrVar (temp->next->next))
                                {
                                    rprin ("Оператор MOD можно использовать только с вещественными числами!");
                                    Undo (); // удалить список лесем
                                    error = true;
                                    return null; // возвратить нулевое комплексное число
                                }
                                goto anew; // перейти к началу списка
//----------------------------------------------------------------------------------------------------------------------
                            case DIV: // целая часть от деления по модулю

                                // если оба числа - вещественные
                                if (IsRealNumberOrVar (temp) &&
                                    IsRealNumberOrVar (temp->next->next))
                                {
                                    // если второе число = 0
                                    if (!(int)temp->next->next->number)
                                    {
                                        rprin("Вы попытались поделить на 0 !");
                                        error = true;
                                        Undo (); // удалить список лесем
                                        return null; // возвратить нулевое комплексное число
                                    }
                                    temp->number = (int)(ABS (((int)temp->number) / ((int)temp->next->next->number)));
                                    // текущую лексему определить как число и записать
                                    // в неё результат
                                    temp->ThisIs = NUMBER;
                                    // удалить две лексемы справа от текущей
                                    Del_2_Elements  ();
                                    goto anew; // перейти к началу списка
                                }

                                // если одно из чисел - комплексное
                                if (IsImNumberOrVar (temp) ||
                                    IsImNumberOrVar (temp->next->next))
                                {
                                    rprin ("Оператор DIV можно использовать только с вещественными числами!");
                                    Undo (); // удалить список лесем
                                    error = true;
                                    return null; // возвратить нулевое комплексное число
                                }
                                goto anew; // перейти к началу списка
//----------------------------------------------------------------------------------------------------------------------
                            case DEGREE: // оператор степени

                                // степень вычисляется методами класса complex
                                a.Re = temp->number;
                                a.Im = temp->Im_number;
                                b.Re = temp->next->next->number;
                                b.Im = temp->next->next->Im_number;
                                temp_complex_number.pow_ (a, b, &temp_complex_number);
                                temp->number = temp_complex_number.Re;
                                temp->Im_number = temp_complex_number.Im;
                                // текущую лексему определить как число и записать
                                // в неё результат
                                temp->ThisIs = NUMBER;
                                // удалить две лексемы справа от текущей
                                Del_2_Elements  ();
                            goto anew; // перейти к началу списка
//----------------------------------------------------------------------------------------------------------------------
                            case MULTIPLICATION: // умножение

                                    if (temp->prior) // если слева от числа есть лексема
                                    {
                                        // и если приоритет этой лексемы выше, например
                                        // слева от четверки оператор степени: 2^4*3
                                        if (LeftOperatorSupremePrioritet ())
                                        {
                                            break; // не вычислять умножение
                                        }
                                    }

                                    // если справа от числа есть третья лексема
                                    if (temp->next->next->next)
                                    {
                                        // и если эта лесема имеет высший приоритет,
                                        // например справа от тройки оператор степени: 4*3^2
                                        if (RightThirdOperatorSupremePrioritet ())
                                        {
                                            break; // не вычислять умножение
                                        }
                                    }

                                    // умножение вычисляется методами класса complex
                                    b.Re = temp->next->next->number;
                                    b.Im = temp->next->next->Im_number;
                                    a.Re = temp->number;
                                    a.Im = temp->Im_number;
                                    a.multiplication (a, b, &c);
                                    temp->number = c.Re;
                                    temp->Im_number = c.Im;
                                    // текущую лексему определить как число и записать
                                    // в неё результат
                                    temp->ThisIs = NUMBER;
                                    // удалить две лексемы справа от текущей
                                    Del_2_Elements  ();
                            goto anew; // перейти к началу списка
//----------------------------------------------------------------------------------------------------------------------
                            case DIVISION: // деление - аналогично умножению, только
                                // проверяется, не равно ли второе число 0

                                if (temp->prior)
                                    if (LeftOperatorSupremePrioritet ())
                                        break;

                                if (temp->next->next->next)
                                    if (RightThirdOperatorSupremePrioritet ())
                                            break;

                                // деление вычисляется методами класса complex
                                b.Re = temp->next->next->number;
                                b.Im = temp->next->next->Im_number;
                                a.Re = temp->number;
                                a.Im = temp->Im_number;
                                a.division (a, b, &c);
                                if (error) // если второе число = 0
                                {
                                    Undo (); // удалить список лесем
                                    return null; // возвратить нулевое комплексное число
                                }
                                temp->number = c.Re;
                                temp->Im_number = c.Im;
                                // текущую лексему определить как число и записать
                                // в неё результат
                                temp->ThisIs = NUMBER;
                                // удалить две лексемы справа от текущей
                                Del_2_Elements  ();
                            goto anew; // перейти к началу списка
//----------------------------------------------------------------------------------------------------------------------
                            case FRACTION: // вычисление дроби вида 3#2 или 1#3#2.
                                // У оператора дробь одинаковый приоритет, что и умножения.

                                if (temp->prior) // если слева от числа есть лексема
                                {
                                    // и если приоритет этой лексемы выше. например
                                    // слева от четверки: 2^4#3
                                    if (LeftOperatorSupremePrioritet ())
                                    {
                                        break; // не вычислять дробь
                                    }
                                }

                                // выяснить, из трех ли чисел состоит дробь

                                // если есть третья лексема справа
                                if (temp->next->next->next)
                                    // если есть третья лексема справа является оператором
                                    // дроби и есть четвертая лексема
                                    if (temp->next->next->next->ThisIs == FRACTION &&
                                        temp->next->next->next->next)
                                        // если четвертая лексема справа является числом
                                        if (IsNumberOrVariable(temp->next->next->next->next))
                                        {
                                            // если есть пятая справа лексема
                                            if (temp->next->next->next->next->next)
                                            {
                                                // если приоритет пятой справа лексемы не
                                                // выше, то вычислить дробь
                                                if (!RightFifthOperatorSupremePrioritet ())
                                                {
                                                    goto frac;
                                                }
                                                else
                                                {
                                                    // иначе перейти к началу списка и
                                                    // продолжить цикл
                                                    temp = temp->next;
                                                    continue;
                                                }
                                            }
                                            else // если дробь - последняя в списке
                                            {
                                                goto frac; // вычислить дробь
                                            }
                                        }

                                // если справа есть 4 лексемы
                                if (temp->next->next &&
                                    temp->next->next->next &&
                                    temp->next->next->next->next)
                                {
                                    // если четвертая лексема - открывающаяся
                                    // скобка или унарный оператор
                                    if (temp->next->next->next->next->ThisIs == BRECKET_OPEN ||
                                        IsUnOperator (temp->next->next->next->next))
                                    {
                                        // иначе перейти к началу списка и продолжить цикл
                                        temp = temp->next;
                                        continue;
                                    }
                                }

                                // остался один вариант - дробь из двух чисел:

                                // проверяется если например справа от четверки:
                                // 2 # 4^3 - степень высшего приоритета, поэтому нельэя
                                // вычислять 2#2, или например exp 2^2 # 2: сначала
                                // вычисляется 2^2, затем ехр 1, затем (ехр 1)#2
                                if (temp->next->next->next)
                                {
                                    // если лексема после второго числа более высокого
                                    // приоритета
                                    if (RightThirdOperatorSupremePrioritet ())
                                        break; // также не вычислять дробь
                                }

                                // дробь типа a#b = a/b
                                if (temp->next->next->next) // без комментария
                                {
                                    if (temp->next->next->next->ThisIs == FRACTION)
                                    {
                                        temp = temp->next->next->next;
                                        goto loop;
                                    }
                                }

                                a.Re = temp->number;
                                a.Im = temp->Im_number;
                                b.Re = temp->next->next->number;
                                b.Im = temp->next->next->Im_number;
                                a.division (a, b, &c);
                                if (error) // если второе число = 0
                                {
                                    Undo (); // удалить список лесем
                                    return null; // возвратить нулевое комплексное число
                                }
                                temp->number = c.Re;
                                temp->Im_number = c.Im;
                                // текущую лексему определить как число и записать
                                // в неё результат
                                temp->ThisIs = NUMBER;
                                // удалить две лексемы справа от текущей
                                Del_2_Elements  ();
                                goto anew; // перейти к началу списка

                                 // a#b#c = a + b/c
    frac:                        // значение дроби равно сумме целой части и отношения числителя и знаменателя дроби
                                a.Re = temp->number;
                                a.Im = temp->Im_number;
                                b.Re = temp->next->next->number;
                                b.Im = temp->next->next->Im_number;
                                c.Re = temp->next->next->next->next->number;
                                c.Im = temp->next->next->next->next->Im_number;
                                temp_complex_number.division (b, c, &temp_complex_number);
                                if (error) // если третье число = 0
                                {
                                    Undo (); // удалить список лесем
                                    return null; // возвратить нулевое комплексное число
                                }
                                temp->number = a.Re + temp_complex_number.Re;
                                temp->Im_number = a.Im + temp_complex_number.Im;
                                // текущую лексему определить как число и записать
                                // в неё результат
                                temp->ThisIs = NUMBER;
                                // убрать два оператора дроби и числитель и знаменатель,
                                // осталось одно значение дроби на месте целой части
                                Del_2_Elements  ();
                                Del_2_Elements  ();
                                goto anew; // перейти к началу списка
//----------------------------------------------------------------------------------------------------------------------
                            case PLUS: // сложение

                                if (temp->prior)
                                {
                                    if (LeftOperatorSupremePrioritet_2 ())
                                    {
                                        break; // не складывать числа
                                    }
                                }

                                if (temp->next->next->next) // в случае 2 + 3*4
                                {
                                    if (RightThirdOperatorSupremePrioritet_2 ())
                                    {
                                        break; // не складывать числа
                                    }
                                }

                                temp->number += temp->next->next->number;
                                temp->Im_number += temp->next->next->Im_number;
                                // текущую лексему определить как число и записать
                                // в неё результат
                                temp->ThisIs = NUMBER;
                                // удалить две лексемы справа от текущей
                                Del_2_Elements  ();
                            goto anew; // перейти к началу списка
//----------------------------------------------------------------------------------------------------------------------
                            case MINUS: // вычитание аналогично что и сложение

                                    if (temp->prior)
                                        if (LeftOperatorSupremePrioritet_2 ())
                                            break;

                                    if (temp->next->next->next)
                                        if (RightThirdOperatorSupremePrioritet_2 ())
                                            break;

                                    temp->number -= temp->next->next->number;
                                    temp->Im_number -= temp->next->next->Im_number;
                                    // текущую лексему определить как число и записать
                                    // в неё результат
                                    temp->ThisIs = NUMBER;
                                    // удалить две лексемы справа от текущей
                                    Del_2_Elements  ();
                            goto anew; // перейти к началу списка
//----------------------------------------------------------------------------------------------------------------------
                            case AND: // логическое И

                                    if (temp->prior)
                                        if (LeftOperatorSupremePrioritet_2 () ||
                                            temp->prior->ThisIs == PLUS ||
                                            temp->prior->ThisIs == MINUS ||
                                            temp->prior->ThisIs == EGALE ||
                                            temp->prior->ThisIs == NE_EGALE ||
                                            temp->prior->ThisIs == GREATER_THAN ||
                                            temp->prior->ThisIs == LESS_THAN ||
                                            temp->prior->ThisIs == GREATER_OR_EQUAL ||
                                            temp->prior->ThisIs == LESS_OR_EQUAL)
                                            break;

                                    if (temp->next->next->next)
                                        if (RightThirdOperatorSupremePrioritet_2 () ||
                                            temp->next->next->next->ThisIs == PLUS ||
                                            temp->next->next->next->ThisIs == MINUS ||
                                            temp->next->next->next->ThisIs == EGALE ||
                                            temp->next->next->next->ThisIs == NE_EGALE ||
                                            temp->next->next->next->ThisIs == GREATER_THAN ||
                                            temp->next->next->next->ThisIs == LESS_THAN ||
                                            temp->next->next->next->ThisIs == GREATER_OR_EQUAL ||
                                            temp->next->next->next->ThisIs == LESS_OR_EQUAL )
                                            break;

                                    temp->number = (temp->number != 0. && temp->next->next->number != 0.);
                                    temp->Im_number = 0.;
                                    // текущую лексему определить как логическое значение
                                    // и записать в неё результат
                                    temp->ThisIs = BOOL;
                                    // удалить две лексемы справа от текущей
                                    Del_2_Elements  ();
                            goto anew; // перейти к началу списка
//----------------------------------------------------------------------------------------------------------------------
                            case OR: // логическое ИЛИ

                                    if (temp->prior)
                                        if (LeftOperatorSupremePrioritet_2 () ||
                                            temp->prior->ThisIs == PLUS ||
                                            temp->prior->ThisIs == MINUS ||
                                            temp->prior->ThisIs == EGALE ||
                                            temp->prior->ThisIs == NE_EGALE ||
                                            temp->prior->ThisIs == GREATER_THAN ||
                                            temp->prior->ThisIs == LESS_THAN ||
                                            temp->prior->ThisIs == GREATER_OR_EQUAL ||
                                            temp->prior->ThisIs == LESS_OR_EQUAL ||
                                            temp->prior->ThisIs == AND)
                                            break;

                                    if (temp->next->next->next)
                                        if (RightThirdOperatorSupremePrioritet_2 () ||
                                            temp->next->next->next->ThisIs == PLUS ||
                                            temp->next->next->next->ThisIs == MINUS ||
                                            temp->next->next->next->ThisIs == EGALE ||
                                            temp->next->next->next->ThisIs == NE_EGALE ||
                                            temp->next->next->next->ThisIs == GREATER_THAN ||
                                            temp->next->next->next->ThisIs == LESS_THAN ||
                                            temp->next->next->next->ThisIs == GREATER_OR_EQUAL ||
                                            temp->next->next->next->ThisIs == LESS_OR_EQUAL ||
                                            temp->next->next->next->ThisIs == AND)
                                            break;

                                    temp->number = (temp->number != 0. || temp->next->next->number != 0.);
                                    temp->Im_number = 0.;
                                    // текущую лексему определить как логическое значение
                                    // и записать в неё результат
                                    temp->ThisIs = BOOL;
                                    // удалить две лексемы справа от текущей
                                    Del_2_Elements  ();
                            goto anew; // перейти к началу списка
//----------------------------------------------------------------------------------------------------------------------
                            case EGALE: // сравнение на равенство

                                    if (temp->prior)
                                        if (LeftOperatorSupremePrioritet_2 () ||
                                            temp->prior->ThisIs == PLUS ||
                                            temp->prior->ThisIs == MINUS)
                                            break;

                                    if (temp->next->next->next)
                                        if (RightThirdOperatorSupremePrioritet_2 () ||
                                            temp->next->next->next->ThisIs == PLUS ||
                                            temp->next->next->next->ThisIs == MINUS )
                                            break;

                                    if (temp->Im_number == 0. && temp->next->next->Im_number == 0.)
                                        temp->number = (temp->number == temp->next->next->number) ? 1. : 0.;
                                    else
                                        temp->number = (temp->number == temp->next->next->number && temp->Im_number == temp->next->next->Im_number);
                                    temp->Im_number = 0.;
                                    // текущую лексему определить как логическое значение
                                    // и записать в неё результат
                                    temp->ThisIs = BOOL;
                                    // удалить две лексемы справа от текущей
                                    Del_2_Elements  ();
                            goto anew; // перейти к началу списка
//----------------------------------------------------------------------------------------------------------------------
                            case NE_EGALE: // сравнение на неравенство

                                    if (temp->prior)
                                        if (LeftOperatorSupremePrioritet_2 () ||
                                            temp->prior->ThisIs == PLUS ||
                                            temp->prior->ThisIs == MINUS)
                                            break;

                                    if (temp->next->next->next)
                                        if (RightThirdOperatorSupremePrioritet_2 () ||
                                            temp->next->next->next->ThisIs == PLUS ||
                                            temp->next->next->next->ThisIs == MINUS )
                                            break;

                                    if (temp->Im_number == 0. && temp->next->next->Im_number == 0.)
                                        temp->number = (temp->number != temp->next->next->number) ? 1. : 0.;
                                    else
                                        temp->number = (temp->number != temp->next->next->number && temp->Im_number != temp->next->next->Im_number);
                                    temp->Im_number = 0.;
                                    // текущую лексему определить как логическое значение
                                    // и записать в неё результат
                                    temp->ThisIs = BOOL;
                                    // удалить две лексемы справа от текущей
                                    Del_2_Elements  ();
                            goto anew; // перейти к началу списка
//----------------------------------------------------------------------------------------------------------------------
                                    case GREATER_THAN: // больше

                                    if (temp->prior)
                                        if (LeftOperatorSupremePrioritet_2 () ||
                                            temp->prior->ThisIs == PLUS ||
                                            temp->prior->ThisIs == MINUS)
                                            break;

                                    if (temp->next->next->next)
                                        if (RightThirdOperatorSupremePrioritet_2 () ||
                                            temp->next->next->next->ThisIs == PLUS ||
                                            temp->next->next->next->ThisIs == MINUS )
                                            break;

                                    temp->number = (temp->number > temp->next->next->number);
                                    temp->Im_number = 0.;
                                    // текущую лексему определить как логическое значение
                                    // и записать в неё результат
                                    temp->ThisIs = BOOL;
                                    // удалить две лексемы справа от текущей
                                    Del_2_Elements  ();
                            goto anew; // перейти к началу списка
//----------------------------------------------------------------------------------------------------------------------
                                    case LESS_THAN: // меньше

                                    if (temp->prior)
                                        if (LeftOperatorSupremePrioritet_2 () ||
                                            temp->prior->ThisIs == PLUS ||
                                            temp->prior->ThisIs == MINUS)
                                            break;

                                    if (temp->next->next->next)
                                        if (RightThirdOperatorSupremePrioritet_2 () ||
                                            temp->next->next->next->ThisIs == PLUS ||
                                            temp->next->next->next->ThisIs == MINUS )
                                            break;

                                    temp->number = (temp->number < temp->next->next->number);
                                    temp->Im_number = 0.;
                                    // текущую лексему определить как логическое значение
                                    // и записать в неё результат
                                    temp->ThisIs = BOOL;
                                    // удалить две лексемы справа от текущей
                                    Del_2_Elements  ();
                            goto anew; // перейти к началу списка
//----------------------------------------------------------------------------------------------------------------------
                                    case GREATER_OR_EQUAL: // больше или равно

                                    if (temp->prior)
                                        if (LeftOperatorSupremePrioritet_2 () ||
                                            temp->prior->ThisIs == PLUS ||
                                            temp->prior->ThisIs == MINUS)
                                            break;

                                    if (temp->next->next->next)
                                        if (RightThirdOperatorSupremePrioritet_2 () ||
                                            temp->next->next->next->ThisIs == PLUS ||
                                            temp->next->next->next->ThisIs == MINUS )
                                            break;

                                    temp->number = (temp->number >= temp->next->next->number);
                                    temp->Im_number = 0.;
                                    // текущую лексему определить как логическое значение
                                    // и записать в неё результат
                                    temp->ThisIs = BOOL;
                                    // удалить две лексемы справа от текущей
                                    Del_2_Elements  ();
                            goto anew; // перейти к началу списка
//----------------------------------------------------------------------------------------------------------------------
                                    case LESS_OR_EQUAL: // меньше или равно

                                    if (temp->prior)
                                        if (LeftOperatorSupremePrioritet_2 () ||
                                            temp->prior->ThisIs == PLUS ||
                                            temp->prior->ThisIs == MINUS)
                                            break;

                                    if (temp->next->next->next)
                                        if (RightThirdOperatorSupremePrioritet_2 () ||
                                            temp->next->next->next->ThisIs == PLUS ||
                                            temp->next->next->next->ThisIs == MINUS )
                                            break;

                                    temp->number = (temp->number <= temp->next->next->number);
                                    temp->Im_number = 0.;
                                    // текущую лексему определить как логическое значение
                                    // и записать в неё результат
                                    temp->ThisIs = BOOL;
                                    // удалить две лексемы справа от текущей
                                    Del_2_Elements  ();
                            goto anew; // перейти к началу списка
//----------------------------------------------------------------------------------------------------------------------
                            // оператор присваивания, скобки и запятую между числами
                            // пропустить
                            case EQUAL:
                                if (temp->next->next->next &&
                                    temp->next->next->next->ThisIs == EQUAL)
                                { // a = b = c
                                    temp=temp->next;
                                    continue;
                                }
                                if (temp->next->next->next &&
                                    temp->next->next->next->ThisIs == COMMA)
                                {
                                    temp = start;
                                     continue;
                                }
                                 //break;
                            case COMMA:
                            case BRECKET_OPEN:
                            case BRECKET_CLOSE:
                            case SQUARE_BRECKET_OPEN:
                            case SQUARE_BRECKET_CLOSE:
                            case COUT2:
                                temp = temp->next; // перейти к следующей лексеме
                                continue;
//----------------------------------------------------------------------------------------------------------------------
                            // некорректное выражение, например, например, три идущих
                            // подряд числа
                            default:
                                rprin ("Синтаксическая ошибка: пропущен оператор!");
                                error = true;
                                Undo (); // удалить список лесем
                                return null; // возвратить нулевое комплексное число

                    } // switch

                } // if (IsNumberOrVariable(temp->next->next))

            } // if (IsNumberOrVariable(temp) && ...

            temp = temp->next; // переход к следующей лексеме

        } // while (temp) // цикл просмотра списка
    } // for (iteration = 0; ; ++iteration)
} // главный метод
//===============================================================================================================
// МЕТОДЫ РАБОТЫ С ДВУСВЯЗНЫМ СПИСКОМ:

// добавляет в конец двусвязного списка новый элемент, если список пустой, то вставляется
// как первый элемент.
void Interpreter::AddInEndVersionShildt (class token *add, class token **end)
{
    if (!*end) // если список пуст
    {
        add->next = add->prior = NULL;
        *end = add;
        return;
    }
    else
        (*end)->next = add;
    add->next = NULL; // обозначить конец списка
    add->prior = *end; // если end нулевой (в списке еще не было элементов),
    // то это утверждение обозначает начало списка,
    //иначе вставляемый элемент будет указывать на конец списка
    *end = add; // переместить указатель на конец массива
}
//=============================================================
// удаляет элемент из двусвязного списка, по необходимости переопределяя указатели на начало
// и конец списка (если удаляется первый или последний элемент).
void Interpreter::DelElementMyVersion    (class token *del,
                                             class token **start,
                                             class token **end)
{
    if (del->prior)
        del->prior->next = del->next;
    else
    {
        // удаление первого элемента
        *start = del->next;
        if (*start) // если элементов больше одного
            (*start)->prior = NULL;
    }
    if (del->next)
        del->next->prior = del->prior;
    else // удаление последнего элемента списка
        *end = del->prior;
    delete del;
}
//=============================================================
// удаление двух элементов справа от текущей лексемы (используется повсеместно)
void Interpreter::Del_2_Elements ()
{
    DelElementMyVersion (temp->next, &start, &end);
    DelElementMyVersion (temp->next, &start, &end);
    countToken -= 2;
}
//=============================================================
// Функция освобождает память от списка, вызывается после завершения вычислений
void Interpreter::DeleteAll (class token **start)
{
    class token *temp, *del;
    temp = *start;
    while (temp) // просмотреть список, по очереди удалив все элементы
    {
        del = temp;
        temp = temp->next;
        delete del;
    }
    *start = NULL;
}

//===============================================================================================================
// МЕТОДЫ ВЫЧИСЛЕНИЯ ОПЕРАТОРОВ:


// Лексема является числом или переменной или булевым значением ?
bool Interpreter::IsNumberOrVariable (class token *ptr)
{
    return  ptr->ThisIs == NUMBER   ||
            ptr->ThisIs == VARIABLE ||
            ptr->ThisIs == BOOL;
}
//=============================================================
void Interpreter::UnMinus () // вычисление минуса числа
{
    if (IsNumberOrVariable (temp) && temp->prior) // если текущая лексема - число и слева есть лексема
        if (temp->prior->ThisIs == MINUS) // если слева лексема - оператор минус
            if (temp->prior->prior) // если перед минусом нет чисел или есть оператор MOD или DIV
            {
                if (!IsNumberOrVariable (temp->prior->prior) &&
                    temp->prior->prior->ThisIs != BRECKET_CLOSE ) // если здесь не вычитание
                {
                    goto unMinus;
                }
            }
            else // если число с минусом первое в списке, например -2 + 3
            {
                goto unMinus;
            }

    return;

unMinus:temp->number *= -1.; // поменять знак комплексной и действительной части числа
        temp->Im_number *= -1.;
        temp->ThisIs = NUMBER;
        DelElementMyVersion (temp->prior, &start, &end); // удалить предыдущую лексему минус
        --countToken; // убрали минус, одной лексемой стало меньше
        temp = start; // перейти к началу списка лексем
}
//=============================================================
void Interpreter::UnPlus () // вычисление плюса числа
{
    if (IsNumberOrVariable (temp) && temp->prior) // аналогично плюс числа, например +2 + 4
        if (temp->prior->ThisIs == PLUS)
            if (temp->prior->prior)
            {
                if (!IsNumberOrVariable (temp->prior->prior) &&
                    temp->prior->prior->ThisIs != BRECKET_CLOSE)
                {
                    goto unPlus;
                }
            }
            else
            {
                goto unPlus;
            }

    return;

unPlus: temp->ThisIs = NUMBER;
        DelElementMyVersion (temp->prior, &start, &end); // удалить предыдущую лексему плюс
        --countToken;
        temp = start; // перейти к началу списка лексем
}
//=============================================================
void Interpreter::Not () // логический оператор отрицания
{
    // если текущая лексема - число и слева есть лексема NOT
    if (IsNumberOrVariable (temp) && temp->prior)
        if (temp->prior->ThisIs == NOT)
        {
            DelElementMyVersion (temp->prior, &start, &end); // удалить лексему NOT
            --countToken; // одной лексемой стало меньше
            temp->Im_number = 0.; // обнулить комплексную часть числа
            temp->number = (temp->number != 0.) ? 0. : 1.; // вычислить отрицание
            temp->ThisIs = BOOL; // тип текущей лексемы указать как логическое значение
            temp = start; // перейти к началу списка лексем
        }
}
//=============================================================
void Interpreter::Factorial () // факториал числа
{
    if (temp->next) // если справа есть лексема
        if (temp->next->ThisIs == FACTORIAL) // и если эта лексема - оператор факториала
        {
            if (IsRealNumberOrVar (temp) ||
                temp->ThisIs == BOOL) // если лексема - вещественное число или логическое значение
            {
                if (temp->number < 0.)
                {
                    error = true;
                    rprin ("Нельзя вычислить факториал отрицательного числа!");
                    return;
                }
                temp->ThisIs = NUMBER;
                temp->number = fact (temp->number); // вычислить факториал. Если значение
                // факториала вышло за пределы допустимых для типа double, флаг error
                // установится в истину

                DelElementMyVersion (temp->next, &start, &end); // удалить лексему факториал
                --countToken; // убрали !, одной лексемой стало меньше
                temp = start;
            }
            else
            {
                error = true;
                rprin ("Нельзя вычислить факториал комплексного числа!");
            }
        }
}
//=============================================================
// удалить лексемы вокруг лексемы ptr. Метод используется для удаления скобок вокруг числа.
void Interpreter::DelBreckets (class token *ptr)
{
    DelElementMyVersion (ptr->next, &start, &end);
    DelElementMyVersion (ptr->prior, &start, &end);
    countToken -= 2; // убрали две скобки
    temp = start;
}
//=============================================================
// удалить скобки вокруг числа, например (2) = 2
void Interpreter::DelBrecketsAroundNumber ()
{
    if ((IsNumberOrVariable (temp) || temp->ThisIs == COUT1) &&
        temp->next && temp->prior) // если найдено число и есть лексемы слева и справа
        if (temp->next->ThisIs  == BRECKET_CLOSE &&
            temp->prior->ThisIs == BRECKET_OPEN) // если лексема слева - открывабщаяся скобка, а лексема справа - закрывающаяся
            if (temp->prior->prior) // если есть вторая лексема слева от числа
            {
                // если вторая лексема слева от числа не имя функции
                if (!IsFunction (temp->prior->prior))
                {
                    DelBreckets (temp); // удалить скобки вокруг числа
                    temp = start;
                }
            }
            else
            {
                DelBreckets (temp); // удалить скобки если слева от открывабщейся нет лексем
                temp = start;
            }
}
//=============================================================
// аналогично удаляем модули вокруг числа, например |2| или |-2|
void Interpreter::DelModulsAroundNumber ()
{
    if (temp->next &&
        temp->next->next) // если справа есть две лексемы
        if (temp->ThisIs == MODULE &&
            IsNumberOrVariable (temp->next) &&
            temp->next->next->ThisIs == MODULE) // если текущая лексема - модуль, следующая справа - число, вторая справа - модуль
                {
                    temp->ThisIs = NUMBER; // на место левого модуля вставляем абс. значение
                    // числа, меняя тип данных лексемы на числовой

                    // модуль вычисляется методами класса complex, поэтому нужно передать
                    // в объект temp_complex_number класса complex число
                    temp_complex_number.Re = temp->next->number;
                    temp_complex_number.Im = temp->next->Im_number;
                    temp->number = temp_complex_number.module (temp_complex_number);
                    temp->Im_number = 0.;
                    temp->ThisIs = NUMBER; // переопределить тип текущей лексемы
                    Del_2_Elements  (); // удалить из списка число и закрывающий модуль
                    temp = start;
                }
}
//=============================================================
bool Interpreter::IsFunction (class token *ptr) // лексема является функцией?
{
    // возвратить истину если лексема является функцией или функцией-оператором
    return  IsFunction1Arg (ptr) || IsFunction2Arg (ptr) ;
}
//===============================================================
bool Interpreter::IsSupremOperator (class token *ptr)
{
    // возвратить истину если лексема является оператором наивысшего приоритета
    return  ptr->ThisIs == DEGREE    || // степень
            ptr->ThisIs == MOD       || // остаток от деления
            ptr->ThisIs == FACTORIAL || // факториал
            ptr->ThisIs == DIV;         // целая часть от деления
}
//=============================================================
bool Interpreter::IsMiddleOperator (class token *ptr)
{
    // возвратить истину если лексема является оператором среднего приоритета
    return  ptr->ThisIs == MULTIPLICATION ||
            ptr->ThisIs == DIVISION       ||
            ptr->ThisIs == FRACTION;
}
//=============================================================
bool Interpreter::IsUnOperator (class token *ptr)
{
    // возвратить истину если лексема является унарным оператором, стоящим перед числом
    return ptr->ThisIs == MINUS || ptr->ThisIs == PLUS || ptr->ThisIs == NOT;
}
//=============================================================
bool Interpreter::OperatorSupremePrioritet (class token *ptr)
{
    // возвратить истину если лексема является оператором высшего приоритета или функцией
    return IsSupremOperator (ptr) || IsFunction (ptr);
}
//=============================================================
bool Interpreter::LeftOperatorSupremePrioritet ()
{
    // возвратить истину если лексема слева является оператором высшего приоритета
    // или функцией
    return OperatorSupremePrioritet (temp->prior);
}
//=============================================================
bool Interpreter::RightThirdOperatorSupremePrioritet ()
{
    // возвратить истину если третья справа лексема является оператором высшего
    // приоритета или функцией
    return OperatorSupremePrioritet (temp->next->next->next);
}
//=============================================================
bool Interpreter::RightFifthOperatorSupremePrioritet () // для дробей из трех чисел
{
    // возвратить истину если пятая справа лексема является оператором высшего
    // приоритета или функцией
    return OperatorSupremePrioritet (temp->next->next->next->next->next);
}
//=============================================================
bool Interpreter::LeftOperatorSupremePrioritet_2 ()
{
    // возвратить истину если лексема слева является оператором высшего или
    // среднего приоритета
    return  IsSupremOperator (temp->prior) || IsMiddleOperator (temp->prior);
}
//=============================================================
bool Interpreter::RightThirdOperatorSupremePrioritet_2 ()
{
    // возвратить истину если третья справа лексема является оператором высшего
    // или среднего приоритета
    return  IsSupremOperator (temp->next->next->next) ||
            IsMiddleOperator (temp->next->next->next);
}
//=============================================================
bool Interpreter::RightSecondOperatorSupremePrioritet ()
{
    // возвратить истину если вторая справа лексема является оператором высшего приоритета
    return  IsSupremOperator (temp->next->next);
}
//=============================================================
bool Interpreter::IsRealNumberOrVar (class token *ptr)
{
    // возвратить истину если текущая лексема является вещественным числом или переменной
    return (ptr->ThisIs == NUMBER || ptr->ThisIs == VARIABLE || ptr->ThisIs == BOOL) &&
            ptr->Im_number == 0.;
}
//=============================================================
bool Interpreter::IsImNumberOrVar (class token *ptr)
{
    // возвратить истину если текущая лексема является комплексным числом или переменной
    return (ptr->ThisIs == NUMBER || ptr->ThisIs == VARIABLE || ptr->ThisIs == BOOL) &&
            ptr->Im_number != 0.;
}
//=============================================================
// Лексема является идентификатором комплексной или действительной части
// значения переменной ?
bool Interpreter::IsMemberStruct (class token *ptr)
{
    return  ptr->ThisIs == MEMBER_STRUCT_RE ||
            ptr->ThisIs == MEMBER_STRUCT_IM;
}
//=============================================================
// функция выполняет блок выражений в фигурных скобках.
// Параметры: объект класса Interpreter и строка содержащая набор выражений,
// разделенных как запятыми, так и точкой с запятой
void Interpreter::run_block (Interpreter *ob, char *str)
{
    char *ptr = str;
    answer.Re = answer.Im = 0;
    for (;;) // пока не закончатся выражения в блоке
    {
        if (!*ptr) // если дошли до конца блока выражений
            break;
        temp_complex_number = ob->interpret (ptr); // вычислить одно выражение, перейти к следующему
        if (error) // если ошибка в выражении
            break;
        if (ThisComment) ThisComment = false;
        else
        {answer = temp_complex_number;}
        if (*(ob->p) == ';') // пропустить разделители выражений
            ++(ob->p);
        ptr = ob->p; // переместить указатели на начало следующего выражения
    }
}
//=============================================================
// метод выполняет конструкцию if, представленную в виде:
// if (условие) {блок выражений 1} else {блок выражений 2});
// или
// if (условие) {блок выражений 1};
// или
// if (условие)
//
// Если условие истинно, выполняется блок выражений 1 , иначе блок выражений 2
// (оба блока могут отсутствовать).
// В условии также может находиться несколько выражений через запятую, в таком случае
// истинность условия находится по последнему выражению, а могут отсутствовать - тогда
// условие будет истинным.
// Все блоки выражений могут быть пустыми.
// Блоки выражений 1 и 2 могут в себя включать любые конструкции условий,
// циклов и т.д.
// Конструкцию if от других частей выражения можно отделять запятыми,
// иначе всегда ставится ;
// Как и любая функция, if возвращает числовое выражение:
//      1 - условие выполнилось,
//      0 - условие ложно
void Interpreter::run_if ()
{
    // если текущая лексема - оператор if
    if (temp->ThisIs == IF)
    {
        // если справа от текущей лексемы есть лексемa
        if (temp->next)
        {
            if (temp->next->ThisIs == STRING) // если лексема справа строка
            {
                Interpreter *action; // объект для вычисления выражений в
                // строковых параметрах
                action = new Interpreter;

                // сделать видимыми в объекте пользовательские переменные, функции, массивы
                action->countVariables = this->countVariables;
                action->countFunctions = this->countFunctions;
                action->countArray     = this->countArray;
                // передать в объект счетчик строк
                action->countString    = this->countString;

                complex current; // результат вычислений

                // вычислить условие
                current = action->interpret (strings[temp->next->IndexString]);
                // удалить из глобального массива strings только что занесенные в него строки
                DeleteStrings (this->countString);
                if (error) // если в условии ошибка
                {
                    // удалить из глобального массива ListArray только что созданные
                    // локальные массивы
                    DelArrays (countArray, action->countArray);
                    DelFunc (countFunctions, action->countFunctions);
                    delete action;
                    return;
                }
                RefreshAllUserData (); // обновить пользовательские данные

                if (return_func) // если встретилась функция return
                {
                    // удалить из глобального массива ListArray только что созданные
                    // локальные массивы
                    DelArrays (countArray, action->countArray);
                    DelFunc (countFunctions, action->countFunctions);
                    delete action;
                    return;
                }

                // если есть вторая лексема справа - строка
                if (temp->next->next && temp->next->next->ThisIs == STRING)
                {
                    if (current.Re || ThisComment) // если условие истинно
                    {
                        // выполнить блок выражений 1
                        run_block (action, strings[temp->next->next->IndexString]);
                        RefreshAllUserData (); // обновить пользовательские данные
                        temp->number = 1.;
                        temp->Im_number = 0.;
                    }
                    else // иначе выполнить блок выражений 2
                    {
                        // если третья и четвертая лексема - else и строка
                        if (temp->next->next->next &&
                            temp->next->next->next->next &&
                            temp->next->next->next->ThisIs == ELSE &&
                            temp->next->next->next->next &&
                            temp->next->next->next->next->ThisIs == STRING)
                        {
                            run_block (action, strings[temp->next->next->next->next->IndexString]);
                            RefreshAllUserData (); // обновить пользовательские данные
                            temp->number = 0.;
                            temp->Im_number = 0.;
                        }
                        // удалить из глобального массива strings только что занесенные
                        // в него строки
                        DeleteStrings (this->countString);
                    }

                } // if (temp->next->next && temp->next->next->ThisIs == STRING)

                    temp->ThisIs = NUMBER; // на место лексемы if записан результат условия
                    DelElementMyVersion (temp->next, &start, &end); // удалить условие
                    --countToken;

                    // если есть блок 1
                    if (temp->next && temp->next->ThisIs == STRING)
                    {
                        DelElementMyVersion (temp->next, &start, &end); // удалить блок 1
                        --countToken;
                        // если есть else и блок 2
                        if (temp->next &&
                            temp->next->next &&
                            temp->next->ThisIs == ELSE &&
                            temp->next->next->ThisIs == STRING)
                        {
                            Del_2_Elements (); // удалить из списка else и блок 2
                        }
                    }

                    // удалить из глобального массива ListArray только что созданные
                    // локальные массивы
                    DelArrays (countArray, action->countArray);
                    DelFunc (countFunctions, action->countFunctions);
                    // удалить из глобального массива strings только что занесенные
                    // в него строки
                    DeleteStrings (this->countString);

                    delete action;
                    return;

            } // if (temp->next->ThisIs == STRING...
            else
            {
                error = true;
            }

        } // if (temp->ThisIs == IF)
    } // if (temp->next
}
//=============================================================
// метод выполняет конструкцию while, представленную в виде:
// while (условие) {тело цикла};
// или
// while (условие);
//
// Тело цикла выполняется пока истинно условие и может отсутствовать.
// В условии также может находиться несколько выражений через запятую, в таком случае
// истинность условия находится по последнему выражению.
// Условие может быть пустыми, тогда цикл становится бесконечным, из которого
// нет выхода (только завершение приложения). Тело цикла может в себя включать любые
// конструкции условий, циклов и т.д.
// Конструкцию while от других частей выражения можно отделять запятыми.
// Как и любая функция, while возвращает числовое выражение: количество итераций цикла.
void Interpreter::run_while ()
{
    // если текущая лексема - оператор while
    if (temp->ThisIs == WHILE)
    {
        // если справа от текущей лексемы есть лексема
        if (temp->next)
        {
            if (temp->next->ThisIs == STRING)
            {
                Interpreter *tmp; // объект для вычисления выражений в
                // строковых параметрах

                tmp = new Interpreter;
                complex current; // текущий результат вычислений

                int loop_iteration = 0; // счетчик итераций цикла
                bool begin = true; // флаг означает, что выполняется первая итерация цикла

                // сделать видимыми в объекте пользовательские переменные, функции, массивы
                tmp->countVariables = this->countVariables;
                tmp->countFunctions = this->countFunctions;
                tmp->countArray = this->countArray;
                // передать в объект счетчик строк
                tmp->countString = this->countString;

                ++count_loop;

                for (;; ++loop_iteration)
                {
                    // вычислить условие цикла
                    current = tmp->interpret (strings[temp->next->IndexString]);
                    // удалить из глобального массива strings только что занесенные
                    // в него строки
                    DeleteStrings (this->countString);
                    // передать в объект счетчик строк
                    tmp->countString = this->countString;
                    if (error) // если в условии ошибка
                    {
                        // удалить из глобального массива ListArray только что созданные
                        // локальные массивы
                        DelArrays (countArray, tmp->countArray);
                        DelFunc (countFunctions, tmp->countFunctions);
                        delete tmp;
                        return;
                    }
                    RefreshAllUserData (); // обновить пользовательские данные

                    if (begin) // если первая итерация цикла
                    {
                        // передать в текущий объект переменные, объявленные в условии
                        this->countVariables = tmp->countVariables;
                        begin = false; // первая итерация цикла пройдена
                    }

                    if (return_func) // если встретилась функция return
                    {
                        // удалить из глобального массива ListArray только что созданные
                        // локальные массивы
                        DelArrays (countArray, tmp->countArray);
                        DelFunc (countFunctions, tmp->countFunctions);
                        delete tmp;
                        return;
                    }

                    if (current.Re == 0. && !ThisComment) // если условие ложно и непустое
                    {
                        break; // завершить цикл
                    }

                    // выполнить тело цикла, если оно есть
                    if (temp->next->next && temp->next->next->ThisIs == STRING)
                    {
                        run_block (tmp, strings[temp->next->next->IndexString]);
                        // удалить из глобального массива strings только что занесенные
                        // в него строки
                        DeleteStrings (this->countString);
                        // передать в объект счетчик строк
                        tmp->countString = this->countString;
                        if (error || return_func) // если в теле ошибка или встретилась
                        // функция return
                        {
                            // удалить из глобального массива ListArray только что созданные
                            // локальные массивы
                            DelArrays (countArray, tmp->countArray);
                            DelFunc (countFunctions, tmp->countFunctions);
                            delete tmp;
                            return;
                        }
                        RefreshAllUserData (); // обновить пользовательские данные
                        if (run_break) // если в теле цикла встретилась команда break
                        {
                            run_break = false; // команда выполнена
                            break; // завершить цикл
                        }
                        if (run_continue)
                            run_continue = false; // команда выполнена
                    }
                } // for (;; ++loop_iteration)
                --count_loop;

                // удалить из глобального массива ListArray только что созданные
                // локальные массивы
                DelArrays (countArray, tmp->countArray);
                DelFunc (countFunctions, tmp->countFunctions);
                // удалить из глобального массива strings только что занесенные в него строки
                DeleteStrings (this->countString);

                DelElementMyVersion (temp->next, &start, &end);
                --countToken; // удалить условие цикла

                 // если есть тело цикла
                if (temp->next && temp->next->ThisIs == STRING)
                {
                    DelElementMyVersion (temp->next, &start, &end);
                    --countToken; // удалить тело цикла
                }

                temp->ThisIs = NUMBER; // на место лексемы while записано число итераций
                temp->number = loop_iteration;
                temp->Im_number = 0.;
                delete tmp;
                return;
            } // if (temp->next->ThisIs == STRING
            else
            {
                error = true;
            }
        } // if (temp->ThisIs == WHILE)
        else
        {
            error = true;
        }
    } // if (temp->next
}
//=============================================================
// метод выполняет конструкцию for (аналогичную в С++):
// for (действия перед циклом; условие; ) {тело цикла});
// или
// for (действия перед циклом; условие; тело цикла);
//
// Действия перед циклом разделенны запятыми, переменные обявленные в них видны и вне цикла.
// Тело цикла (выражения, разделенные точкой с запятой) а затем действия после тела цикла
// (выражения, разделенные запятой) выполняется пока истинно условие.
// В условии также может находиться несколько выражений через запятую, в таком случае
// истинность условия находится по последнему выражению.
// Все наборы выражений могут быть пустыми, если пустое условие, то цикл становится
// бесконечным, из которого нет выхода (только завершение приложения).
// Тело цикла может в себя включать любые конструкции условий, циклов и т.д.
// Конструкцию for от других частей выражения можно отделять запятыми.
// Как и любая функция, for возвращает числовое выражение: количество итераций цикла.
void Interpreter::run_for ()
{
    if (temp->ThisIs == FOR) // если текущая лексема - оператор for
    {
        if (temp->next) // если справа от текущей лексемы есть лексема
        {
            // если справа от текущей лексемы есть лексема - cтрока
            if (temp->next->ThisIs == STRING)
            {
                Interpreter *action; // объект для вычисления выражений в
                // строковых параметрах
                action = new Interpreter;

                // сделать видимыми в объекте пользовательские переменные, функции, массивы
                action->countVariables = this->countVariables;
                action->countFunctions = this->countFunctions;
                action->countArray = this->countArray;
                // передать в объект счетчик строк
                action->countString = this->countString;

                char *ptrConditionLoop, // указатель на начало условия цикла
                     *ptrActionAfterCondition; // указатель на начало действий после
                                               // тела цикла

                // указатель переместить на начало действий перед циклом
                ptrConditionLoop = &strings[temp->next->IndexString][0];
                // выполнить выражение перед циклом.
                answer = action->interpret (strings[temp->next->IndexString]);
                RefreshAllUserData (); // обновить пользовательские данные
                // вычислить одно выражение, перейти к следующему:
                if (*(action->p) == ';') // пропустить разделители выражений
                    ++(action->p);

                // переместить указатель на начало условия цикла
                ptrConditionLoop = action->p;
                // удалить из глобального массива strings только что занесенные в него строки
                DeleteStrings (this->countString);
                // передать в объект счетчик строк
                action->countString = this->countString;

                // если нашлась ошибка или встретилась функция return
                if (error || return_func)
                {
                    // удалить из глобального массива ListArray только что созданные
                    // локальные массивы
                    DelArrays (countArray, action->countArray);
                    DelFunc (countFunctions, action->countFunctions);
                    delete action;
                    return;
                }

                this->countVariables = action->countVariables;

                complex current; // текущий результат вычислений
                int loop_iteration = 0; // счетчик итераций цикла
                bool begin = true; // флаг означает, что выполняется первая итерация цикла

                ++count_loop; // счетчик вложенности циклов, необходим для выполнения
                // команд break и continue

                // флаг показывает, есть ли тело цикла
                bool ExistBodyLoop = (temp->next->next && temp->next->next->ThisIs == STRING);

                for (;; ++loop_iteration)
                {
                    // вычислить условие цикла
                    current = action->interpret (ptrConditionLoop);
                    // удалить из глобального массива strings только что занесенные
                    // в него строки
                    DeleteStrings (this->countString);
                    // передать в объект счетчик строк
                    action->countString = this->countString;
                    if (error) // если в условии ошибка
                    {
                        // удалить из глобального массива ListArray только что созданные
                        // локальные массивы
                        DelArrays (countArray, action->countArray);
                        DelFunc (countFunctions, action->countFunctions);
                        delete action;
                        return;
                    }
                    RefreshAllUserData (); // обновить пользовательские данные

                    if (begin) // если первая итерация цикла
                    {
                        // передать в текущий объект переменные, объявленные в условии
                        this->countVariables = action->countVariables;
                        if (*(action->p) == ';') // пропустить разделители выражений
                            ++(action->p);
                        // указатель на начало действий после тела цикла
                        ptrActionAfterCondition = action->p;
                        begin = false; // первая итерация цикла пройдена
                    }

                    if (return_func) // если встретилась функция return
                    {
                        // удалить из глобального массива ListArray только что созданные
                        // локальные массивы
                        DelArrays (countArray, action->countArray);
                        DelFunc (countFunctions, action->countFunctions);
                        delete action;
                        return;
                    }

                    if (current.Re == 0. && !ThisComment) // если условие ложно и непустое
                    {
                        break;
                    }

                    if (ExistBodyLoop) // если есть тело цикла, выполнить его
                    {
                        run_block (action, strings[temp->next->next->IndexString]);
                        // удалить из глобального массива strings только что занесенные
                        // в него строки
                        DeleteStrings (this->countString);
                        // передать в объект счетчик строк
                        action->countString = this->countString;
                        // если ошибка или встретилась функция return
                        if (error || return_func)
                        {
                            // удалить из глобального массива ListArray только что созданные
                            // локальные массивы
                            DelArrays (countArray, action->countArray);
                            DelFunc (countFunctions, action->countFunctions);
                            delete action;
                            return;
                        }
                        RefreshAllUserData (); // обновить пользовательские данные
                        if (run_break)
                        {
                            run_break = false;
                            break;
                        }
                        if (run_continue)
                        {
                            run_continue = false;
                            continue;
                        }
                    }
                    // выполнить действия после тела цикла
                    action->interpret (ptrActionAfterCondition);
                    // удалить из глобального массива strings только что занесенные
                    // в него строки
                    DeleteStrings (this->countString);
                    // передать в объект счетчик строк
                    action->countString = this->countString;
                    if (error || return_func)
                    {
                        // удалить из глобального массива ListArray только что созданные
                        // локальные массивы
                        DelArrays (countArray, action->countArray);
                        DelFunc (countFunctions, action->countFunctions);
                        delete action;
                        return;
                    }
                    RefreshAllUserData (); // обновить пользовательские данные
                    if (run_break)
                    {
                        run_break = false;
                        break;
                    }
                    if (run_continue)
                    {
                        run_continue = false;
                    }

                } // for (;; ++loop_iteration)
                --count_loop;

                DelArrays (countArray, action->countArray);
                DelFunc (countFunctions, action->countFunctions);
                // удалить из глобального массива strings только что занесенные в него строки
                DeleteStrings (this->countString);

                DelElementMyVersion (temp->next, &start, &end);
                --countToken; // удалить блок внутри круглых скобок
                if (temp->next && temp->next->ThisIs == STRING)
                {
                    DelElementMyVersion (temp->next, &start, &end); // удалить тело цикла
                    --countToken;
                }
                temp->ThisIs = NUMBER;
                temp->Im_number = 0.;
                // на место лексемы for записано число итераций
                temp->number = loop_iteration;
                delete action;
                return;
            } // if (temp->next->ThisIs == BRECKET_OPEN && ...
            else
            {
                error = true;
            }
        } // if(temp->next && ...
        else
        {
            error = true;
        }
    } // if (temp->ThisIs == FOR)
}
//=============================================================
// метод выполняет конструкцию цикла с предусловием:
// do {тело цикла} while (условие);
//
// Тело цикла (выражения, разделенные точкой с запятой) выполняется хотя бы один раз,
// а затем пока истинно условие (цикл с предусловием - как в языке С++)
void Interpreter::run_dowhile ()
{
    if (temp->ThisIs == DO) // если текущая лексема оператор do
    {
        // если справа от текущей лексемы есть 3 лексемы
        if (
        temp->next &&
        temp->next->next &&
        temp->next->next->next)
        {
            if (temp->next->ThisIs == STRING &&
                temp->next->next->ThisIs == WHILE &&
                temp->next->next->next->ThisIs == STRING)
            {
                Interpreter *tmp; // объект для вычисления выражений в
                // строковых параметрах
                tmp = new Interpreter;

                complex current; // текущий результат вычислений
                int loop_iteration = 0; // счетчик итераций цикла
                bool begin = true; // флаг означает, что выполняется первая итерация цикла

                // сделать видимыми в объекте пользовательские переменные, функции, массивы
                tmp->countVariables = this->countVariables;
                tmp->countFunctions = this->countFunctions;
                tmp->countArray = this->countArray;
                // передать в объект счетчик строк
                tmp->countString = this->countString;

                ++count_loop;
                for (;; ++loop_iteration)
                {
                    // выполнить тело цикла
                    run_block (tmp, strings[temp->next->IndexString]);
                    // удалить из глобального массива strings только что занесенные
                    // в него строки
                    DeleteStrings (this->countString);
                    // передать в объект счетчик строк
                    tmp->countString = this->countString;
                    if (error || return_func) // если нашлась ошибка или встретилась функция return
                    {
                        // удалить из глобального массива ListArray только что созданные
                        // локальные массивы
                        DelArrays (countArray, tmp->countArray);
                        DelFunc (countFunctions, tmp->countFunctions);
                        delete tmp;
                        return;
                    }
                    RefreshAllUserData (); // обновить пользовательские данные
                    if (run_break) // если в теле цикла встретилась команда break
                    {
                        run_break = false;
                        break;
                    }
                    if (run_continue) // если в теле цикла встретилась команда continue
                        run_continue = false;

                    // вычислить условие цикла
                    current = tmp->interpret (strings[temp->next->next->next->IndexString]);
                    // удалить из глобального массива strings только что занесенные в него
                    // строки
                    DeleteStrings (this->countString);
                    tmp->countString = this->countString;
                    if (error)
                    {
                        // удалить из глобального массива ListArray только что созданные
                        // локальные массивы
                        DelArrays (countArray, tmp->countArray);
                        DelFunc (countFunctions, tmp->countFunctions);
                        delete tmp;
                        return;
                    }
                    RefreshAllUserData (); // обновить пользовательские данные

                    if (begin) // если первая итерация цикла
                    {
                        // передать в текущий объект переменные, объявленные в условии
                        this->countVariables = tmp->countVariables;
                        begin = false; // первая итерация цикла пройдена
                    }

                    if (return_func)
                    {
                        // удалить из глобального массива ListArray только что созданные
                    // локальные массивы
                    DelArrays (countArray, tmp->countArray);
                    DelFunc (countFunctions, tmp->countFunctions);
                        delete tmp;
                        return;
                    }

                    if (current.Re == 0.&& !ThisComment) // если условие ложно и непустое
                    {
                        break;
                    }
                }
                --count_loop;

                DelArrays (countArray, tmp->countArray);
                DelFunc (countFunctions, tmp->countFunctions);
                // удалить из глобального массива strings только что занесенные в него строки
                DeleteStrings (this->countString);

                Del_2_Elements ();
                DelElementMyVersion (temp->next, &start, &end);
                --countToken; // удалить 3 лексемы после do
                temp->ThisIs = NUMBER;
                // на место лексемы do записано число итераций цикла
                temp->number = loop_iteration;
                temp->Im_number = 0.;
                delete tmp;
                temp = start;
                return;
            } // if (temp->next->ThisIs == STRING && ...
            else
            {
                error = true;
            }
        } // if (temp->next && ...
        else
        {
            error = true;
        }
    } // if (temp->ThisIs == DO)
}
//=============================================================
// Метод выполняет оператор cout, внешне напоминающий вывод в языке С++:
// cout << аргумент;
//
// Идентификатор cout подобен потоковой переменной cout из С++ и не должен применяться в
// арифметических выражениях.
// Аргумент может быть числом, логическим значением или строкой.
// Оператор << с помощью метода write записывает аргумент в поток и возвращает идентификатор
// cout, сам по себе cout без оператора << и аргумента переводится в нулевое комплексное
// число.
void Interpreter::run_cout ()
{
    if (temp->ThisIs == COUT1) // если текущая лексема - идентификатор cout
    {
        // если справа от cout нет лексем
        if (!temp->next)
        {
            return;
        }

        // если следующая лексема не закрывающаяся скобка, не запятая и не оператор <<.
        // идентификатор cout может быть один, без << и второго операнда.
        if (temp->next->ThisIs != BRECKET_CLOSE &&
            temp->next->ThisIs != COMMA &&
            temp->next->ThisIs != COUT2)
        {
            error = true;
            rprin ("Правильное написание: cout << argument;");
            return;
        }

        // Если справа от cout есть третья лексема.
        // Конструкция вывода выполняется только когда после второго операнда
        // закрывающаяся скобка, запятая, оператор << или ничего (конец списка лексем).
        if (temp->next->next && temp->next->next->next)
        {
            if (temp->next->next->next->ThisIs != BRECKET_CLOSE &&
                temp->next->next->next->ThisIs != COMMA &&
                temp->next->next->next->ThisIs != COUT2)
            {
                return;
            }
        }

        // если справа от cout оператор <<
        if (temp->next->ThisIs == COUT2)
        {
            // неправильный второй операнд - не число и не строка
            if (!(temp->next->next) || !IsNumberOrVariable(temp->next->next) &&
                temp->next->next->ThisIs != STRING)
            {
                error = true;
                rprin ("cout << argument-число или строка!");
                return;
            }
            // вывести данные из второго операнда - число или строку
            write (result, temp->next->next);
            Del_2_Elements (); // удалить << и второй операнд

            // если справа от cout запятая
            if (temp->next && temp->next->ThisIs == COMMA)
            {
                if (!temp->next->next) // а после запятой ничего нет
                {
                    error = true;
                    rprin ("Лишняя запятая после cout");
                    return;
                }
                // удалить cout и запятую
                DelElementMyVersion (temp->next, &start, &end);
                DelElementMyVersion (temp, &start, &end);
                countToken -= 2;
            }
        } // if (temp->next->ThisIs == COUT2)
        else
            if (temp->next->ThisIs == COMMA) // если справа от cout запятая
            {
                if (!temp->next->next) // а после запятой ничего нет
                {
                    error = true;
                    rprin ("Лишняя запятая после cout");
                    return;
                }

                // удалить cout и запятую
                DelElementMyVersion (temp->next, &start, &end);
                DelElementMyVersion (temp, &start, &end);
                countToken -= 2;
            }
        temp = start; // возвратиться к началу списка лексем - обязательно!
    }
}

//===============================================================================================================
// МЕТОДЫ ВЫЧИСЛЕНИЯ ФУНКЦИЙ ОТ ОДНОГО АРГУМЕНТА:

bool Interpreter::IsFunction1Arg (class token *ptr)
{
    // возвратить истину если лексема является функцией от одного аргумента
    return      ptr->ThisIs == EXP              ||
                ptr->ThisIs == LN               ||
                ptr->ThisIs == SIN              ||
                ptr->ThisIs == COS              ||
                ptr->ThisIs == TG               ||
                ptr->ThisIs == CTG              ||
                ptr->ThisIs == ABSVALUE         ||
                ptr->ThisIs == ASIN             ||
                ptr->ThisIs == ACOS             ||
                ptr->ThisIs == ATAN             ||
                ptr->ThisIs == ACTAN            ||
                ptr->ThisIs == SQRT             ||
                ptr->ThisIs == SQR              ||
                ptr->ThisIs == CUB              ||
                ptr->ThisIs == CBRT             ||
                ptr->ThisIs == ROUND            ||
                ptr->ThisIs == LG               ||
                ptr->ThisIs == ARG              ||
                ptr->ThisIs == COMPLEX_MODULE   ||
                ptr->ThisIs == CONJ             ||
                ptr->ThisIs == SH_              ||
                ptr->ThisIs == CH               ||
                ptr->ThisIs == TH               ||
                ptr->ThisIs == ASH              ||
                ptr->ThisIs == ACH              ||
                ptr->ThisIs == ATH              ||
                ptr->ThisIs == CTH              ||
                ptr->ThisIs == ACTH             ||
                ptr->ThisIs == SEC              ||
                ptr->ThisIs == CSEC             ||
                ptr->ThisIs == TORAD            ||
                ptr->ThisIs == TODEG            ||
                ptr->ThisIs == WRITE            ||
                ptr->ThisIs == WRITELN          ||
                ptr->ThisIs == RETURN           ||
                ptr->ThisIs == GETRE            ||
                ptr->ThisIs == GETIM            ||
                ptr->ThisIs == RUN              ||
                ptr->ThisIs == EXACT;
}
//=============================================================
// вызывает целый ряд методов для определения и вычисления конкретной функции
void Interpreter::Functions_Of1Arguments ()
{
    // если справа от текущей лексемы есть три лексемы
    if (temp->next &&
        temp->next->next &&
        temp->next->next->next)
    {
        // и если справа от текущей первая лексема - открывающаяся скобка,
        // вторая - число или строка, третья - закрывающаяся скобка
        if (temp->next->ThisIs == BRECKET_OPEN &&
            (IsNumberOrVariable(temp->next->next) ||
            temp->next->next->ThisIs == STRING) &&
            temp->next->next->next->ThisIs == BRECKET_CLOSE)
        {

            // выяснить, является ли текущая лексема именем функции, и если
            // нелбходимо, выполнить её
            Function_Of1Argument();

        }
    }
}
//=============================================================
// выполняет функцию или функцию-оператор. Параметр ptr_arg указывает на аргумент функции,
// n равно 1 если нужно выполнить функцию-оператор или 3 в случае обычной функции.
void Interpreter::RunFunction1Arg (class token *ptr_arg, int n)
{
    if (IsNumberOrVariable(ptr_arg) /*&& ptr_arg->ThisIs!=STRING*/ &&
        temp->ThisIs != WRITE &&
        temp->ThisIs != WRITELN &&
        temp->ThisIs != RUN) // если аргумент не строковый и не функции write и writeln ...
    {

        // следующие функции вычисляются методами класса complex, поэтому аргумент
        // копируются в объект temp_complex_number класса complex
        temp_complex_number.Re = ptr_arg->number;
        temp_complex_number.Im = ptr_arg->Im_number;
        switch (temp->ThisIs)
        {
        case EXP:           temp_complex_number.exp_                (&temp_complex_number); break; // экспонента
        case ABSVALUE:      temp_complex_number.complex_module      (&temp_complex_number); break; // модуль
        case LN:            temp_complex_number.ln_                 (&temp_complex_number); break; // натуральный логарифм
        case SIN:           temp_complex_number.sin_                (&temp_complex_number); break; // синус
        case COS:           temp_complex_number.cos_                (&temp_complex_number); break; // косинус
        case TG:            if (ptr_arg->number &&
                                !(ptr_arg->number / 90. - (int)ptr_arg->number / 90) &&
                                ptr_arg->Im_number == 0.)
                            {
                                rprin ("Тангенса +/-90 градусов не существует!");
                                error = true;
                                return;
                            }
                            temp_complex_number.tg_                 (&temp_complex_number); break; // тангенс
        case CTG:           if ((ptr_arg->number == 0. ||
                                !(ptr_arg->number / 180. - (int)ptr_arg->number / 180)) &&
                                ptr_arg->Im_number == 0.)
                            {
                                rprin ("Koтангенса +/-180 или 0 градусов не существует!");
                                error = true;
                                return;
                            }
                                temp_complex_number.ctg_            (&temp_complex_number); break; // котангенс
        case ACOS:              temp_complex_number.acos_           (&temp_complex_number); break; // арккосинус
        case ASIN:              temp_complex_number.asin_           (&temp_complex_number); break; // арксинус
        case ATAN:          if (ptr_arg->number == 0. &&
                                ((ptr_arg->Im_number) == 1. ||
                                ptr_arg->Im_number == -1.))
                            {
                                error = true;
                                rprin ("Atg(+/-i) не существует!");
                                return;
                            }
                                temp_complex_number.atg_            (&temp_complex_number); break; // арктангенс

        case ACTAN:         if (ptr_arg->number == 0. &&
                                ((ptr_arg->Im_number) == 1. ||
                                ptr_arg->Im_number == -1.))
                            {
                                error = true;
                                rprin ("Actg(+/-i) не существует!");
                                return;
                            }
                                temp_complex_number.actg_           (&temp_complex_number); break; // арккотангенс
        case SQRT:              temp_complex_number.sqrt_           (&temp_complex_number); break; // кв. корень
        case SQR:               temp_complex_number.sqr_            (&temp_complex_number); break; // квадрат
        case CUB:               temp_complex_number.cub_            (&temp_complex_number); break; // куб
        case CBRT:              temp_complex_number.cbrt_           (&temp_complex_number); break; // куб. корень
        case ROUND:             temp_complex_number.round_          (&temp_complex_number); break; // округление
        case LG:                temp_complex_number.log10_          (&temp_complex_number); break; // десятичный логарифм
        case ARG:               temp_complex_number.complex_arg     (&temp_complex_number); break; //
        case COMPLEX_MODULE:    temp_complex_number.complex_module  (&temp_complex_number); break; //
        case CONJ:              temp_complex_number.conj_           (&temp_complex_number); break; //
        case SH_:               temp_complex_number.sh_             (&temp_complex_number); break;
        case CH:                temp_complex_number.ch_             (&temp_complex_number); break;
        case TH:                temp_complex_number.th_             (&temp_complex_number); break;
        case CTH:               temp_complex_number.cth_            (&temp_complex_number); break;
        case ASH:               temp_complex_number.ash_            (&temp_complex_number); break;
        case ACH:               temp_complex_number.ach_            (&temp_complex_number); break;
        case ATH:               temp_complex_number.ath_            (&temp_complex_number); break;
        case ACTH:              temp_complex_number.acth_           (&temp_complex_number); break;
        case SEC:               temp_complex_number.sec_            (&temp_complex_number); break;
        case CSEC:              temp_complex_number.csec_           (&temp_complex_number); break;
        case TORAD:             temp_complex_number.torad_          (&temp_complex_number); break;
        case TODEG:             temp_complex_number.todeg_          (&temp_complex_number); break;
        case RETURN:            return_value = temp_complex_number;
                                return_func = true;
                                ThisComment = false;
                                return;
                                break;
        case GETRE:             temp_complex_number.getre_          (&temp_complex_number); break;
        case GETIM:             temp_complex_number.getim_          (&temp_complex_number); break;
        }
    }
    else // если функции write или writeln или run
        {
            temp_complex_number.Im = temp_complex_number.Re = 0.;

            switch (temp->ThisIs)
            {
            case WRITE:
                temp_complex_number.Re = write (result, ptr_arg);
                break;

            case WRITELN:
                temp_complex_number.Re = writeln (result, ptr_arg);
                break;

            case RUN:
                temp_complex_number = run_Function (ptr_arg);
                break;
            default:
                error=true;
                rprin ("Функция не поддерживает строковый аргумент ");
            }
        }

        if (!error) // если нет ошибок
        {
            temp->number = temp_complex_number.Re;
            temp->Im_number = temp_complex_number.Im;

            // если функция exact
            if (temp->ThisIs == EXACT)
            {
                // записать в глобальную переменную exact точность отображения результата.
                exact = ptr_arg->number;
                temp->Im_number = 0;

                // точность должна быть в пределах от 0 до MAX_EXACT
                if (exact < 0)
                    exact = 0;
                else
                    if (exact > MAX_EXACT)
                        exact = MAX_EXACT;
                rprin ("Результат отображается с точностью ");
                if (exact)
                    printf ("%i\n", exact);
                else
                    rputs ("по умолчанию");
                temp->number = exact; // Функция exact возвращает точность.
            }

            temp->ThisIs = NUMBER; // переопределить тип данных лексемы в числовой,
            // на место названия функции записав результат

            countToken -= n;            // теперь лексем в списке стало на n меньше

            // удалить из списка лексемы с аргументом и скобками вокруг него
            for (int i=0; i < n; i++, DelElementMyVersion (temp->next, &start, &end));

            temp = start; // перейти к началу списка лексем
        }
}
//=============================================================
void Interpreter::Function_Of1Argument ()
{
    if (IsFunction1Arg (temp)) // если найдена функция от одного аргумента,
        RunFunction1Arg (temp->next->next, 3); // выполнить её
}

// Метод выполняет функцию write - записывает в файл out - Результат.тхт данные,
// хранящиеся в ptr_arg. Возвращает количество записаных байт.
int Interpreter::write (FILE *out, class token *ptr_arg)
{
    if (!out) // файл не открыт
    {
        //remove ("Код.txt");
        FatalError ("Файл \"Результат.txt\" не существует или нет доступа.");
    }

    int bytes = 0; // счетчик байт

    switch (ptr_arg->ThisIs) // в зависимости чем является лексема ptr_arg
    {
    case BOOL: // логическое значение
        bytes = (ptr_arg->number) ? fprintf (out, "true") : fprintf (out, "false");
        break;

    case NUMBER: // число или переменная (тип всех переменных - численный)
    case VARIABLE:
        bytes = WriteComplexNumberInFile (  ptr_arg->number, // действительная часть числа
                                            ptr_arg->Im_number, // комплексная часть
                                            out, // выходной файл
                                            exact // точность - число знаков после запятой
                                          );
        break;

    case STRING: // текстовая строка
         // записать строку из массива strings, индекс которой хранится в ptr_arg
#ifdef WINDOWS
        bytes = fprintf (out, strings[ptr_arg->IndexString]);
#else
        char *ptemp = get_utf8 (strings[ptr_arg->IndexString]);
        bytes = fprintf (out, "%s", ptemp);
        delete ptemp;

#endif
        break;
    }
    Write = true; // флаг определяет, производилась ли запись, перед вызовом данного
    // метода равен false. Флаг используется в методе NotepadRegime. Если равно false,
    // выходной файл не будет открываться для пользователя

    fflush (out);
    return bytes; // возвратить число записаных байт
}
//=============================================================
// та же функция, только дозаписывает символ новой строки
int Interpreter::writeln (FILE *out, class token *ptr_arg)
{
    if (!out) // файл не открыт
        return 0;
    return (write (out, ptr_arg) + fprintf (out, "\n"));
}
//=============================================================



// Функции-операторы подобны обычным функциям с одним аргументом и отличаются тем,
// что являются агрегатными операторами и для них есть место в таблице приоритетов
// операторов, а значит они вычисляются по строгим правилам. Например,
// sin 30 + 1 = sin(30) + 1 = 1.5 -  в первом случае используется оператор синус
// числа, а во втором - функция, результат тот же так как приоритет оператора плюс
// меньше оператора синуса. С другой строны, sin 30 ^ 2 + 1 = 1, sin(30) ^ 2 + 1 = 1.25.
// В принципе функции-операторы являются излишними в данной программе, но я их оставил
// с целью уменьшения количества скобок в коротких выражениях.


// вызывает целый ряд методов для определения и вычисления конкретной функции-оператора
void Interpreter::Functions_Operators_Of1Arguments ()
{
    if (temp->next) // если есть справа лексема
        if (IsNumberOrVariable (temp->next) ||
            temp->next->ThisIs == STRING) // и если эта лексема - число, логическое значение, переменная или строка
            if (temp->next->next) // если после числа есть лексема
            {

                // если второй справа оператор имеет меньший приоритет, например
                // ехр 2*2 - эквивалентно (ехр 2)*2
                if (!RightSecondOperatorSupremePrioritet ())
                    Function_OperatorOf1Argument (); // если temp - функция, вычислить функцию
            }
            else // если после числа нет лексем, точно необходимо вычислить функцию
                Function_OperatorOf1Argument ();
}
//=============================================================
// вызывает, в зависимости от типа лексемы, одну из функций от одного аргумента
void Interpreter::Function_OperatorOf1Argument ()
{
    if (IsFunction1Arg (temp))
    {
        RunFunction1Arg (temp->next, 1);
    }
}

// Пример для функций-операторов: ДО ВЫЗОВА ФУНКЦИИ
//      лексема а:      лексема b:      лексема с:
//
//      ЕХР             NUMBER          ...
//      <нет записи>    1               ...
//      next -> b       next -> с       next -> ...
//      ... <- prior    a <- prior      b <- prior
//
//       ПОСЛЕ ВЫЗОВА ФУНКЦИИ
//      лексема а:      лексема c:  (лексема b удалена)
//
//      NUMBER          ...
//      2.712
//      next -> c       next -> ...
//      ... <- prior    a <- prior}
// Для всех функций единый алгоритм.






//=============================================================
// МЕТОДЫ ВЫЧИСЛЕНИЯ ФУНКЦИЙ ОТ ДВУХ АРГУМЕНТОВ:

bool Interpreter::IsFunction2Arg (class token *ptr)
{
    // возвратить истину если лексема является функцией от двух аргументов
    return      ptr->ThisIs == HYPOT    ||
                ptr->ThisIs == LOG      ||
                ptr->ThisIs == RAND     ||
                ptr->ThisIs == MIDDLE   ||
                ptr->ThisIs == MAX2     ||
                ptr->ThisIs == MIN2     ||
                ptr->ThisIs == NOD_DEL  ||
                ptr->ThisIs == POW;
}
//===============================================================
// Лексема является функцией от двух аргументов?
bool Interpreter::IsFunctionOf2Arguments ()
{
    if (temp->next &&
        temp->next->next &&
        temp->next->next->next &&
        temp->next->next->next->next &&
        temp->next->next->next->next->next) // если справа есть 5 лексем
        if (temp->next->ThisIs == BRECKET_OPEN &&
            IsNumberOrVariable (temp->next->next) &&
            temp->next->next->next->ThisIs == COMMA &&
            IsNumberOrVariable (temp->next->next->next->next) &&
            temp->next->next->next->next->next->ThisIs == BRECKET_CLOSE)
            return true;
    return false;
}
//=============================================================
// вызывает целый ряд методов для определения и вычисления конкретной функции.
void Interpreter::Functions_Of2Arguments ()
{
    // если текущая лексема - функции от двух аргументов
    if (IsFunction2Arg (temp))
    {
        if (IsFunctionOf2Arguments ())
        {
            // в зависимости от имени функции выбрать нужную функцию
            switch (temp->ThisIs)
            {
                case HYPOT:     hypot   (); break;
                case LOG:       log_    (); break;
                case RAND:      rand_   (); break;
                case MIDDLE:    middle  (); break;
                case MAX2:      max_    (); break;
                case MIN2:      min_    (); break;
                case NOD_DEL:   nod     (); break;
                case POW:       pow_    (); break;
            }
            // в лексему имени функции записать результат вычислений,
            // тип хранимых данных поменять на числовой
            temp->ThisIs = NUMBER;
            DelElementMyVersion (temp->next, &start, &end); // удалить из списка открывающуюся скобку
            DelElementMyVersion (temp->next, &start, &end); // удалить из списка первый аргумент
            DelElementMyVersion (temp->next, &start, &end); // удалить из списка запятую-разделитель аргументов
            DelElementMyVersion (temp->next, &start, &end); // удалить из списка второй аргумент
            DelElementMyVersion (temp->next, &start, &end); // удалить из списка закрывающуюся скобку
            countToken -= 5; // пятью лексемами стало меньше
            temp = start; // перейти к началу списка лексем
        }
    }
}
//=============================================================
void Interpreter::hypot ()
{
    complex compl2;
    temp_complex_number.Re = temp->next->next->number;
    temp_complex_number.Im = temp->next->next->Im_number;
    compl2.Re = temp->next->next->next->next->number;
    compl2.Im = temp->next->next->next->next->Im_number;
    temp_complex_number.hypot_ (&temp_complex_number, &compl2);
    temp->number = temp_complex_number.Re;
    temp->Im_number = temp_complex_number.Im;
}
//=============================================================
void Interpreter::log_ ()
{
    // основание логарифма рано единице - абсурдно! Единица в любой степени равна единице.
    if (temp->next->next->next->next->number == 1. &&
        temp->next->next->next->next->Im_number == 0.)
    {
        rprin ("Основание логарифма не должно быть равным единице!");
        error = true;
        return;
    }

// логарифм по произвольному основанию равен отношению натуральных логарифмов
// аргумента и основания
    complex compl2;
    temp_complex_number.Re = temp->next->next->number;
    temp_complex_number.Im = temp->next->next->Im_number;
    compl2.Re = temp->next->next->next->next->number;
    compl2.Im = temp->next->next->next->next->Im_number;
    temp_complex_number.log_ (&temp_complex_number, &compl2);
    temp->number = temp_complex_number.Re;
    temp->Im_number = temp_complex_number.Im;
}
//=============================================================
void Interpreter::rand_ ()
{
    if (temp->next->next->Im_number != 0. ||
        temp->next->next->next->next->Im_number != 0.)
    {
        error = true;
        rprin ("Функция rand генерирует только вещественные числа!");
    }
    temp->number = temp->next->next->number +
                   (temp->next->next->next->next->number -
                   temp->next->next->number) / RAND_MAX * rand ();
}
//=============================================================
void Interpreter::middle ()
{
    temp->number = (temp->next->next->number + temp->next->next->next->next->number) / 2.;
    temp->Im_number = (temp->next->next->Im_number + temp->next->next->next->next->Im_number) / 2.;
}
//=============================================================
void Interpreter::max_ ()
{
    if (temp->next->next->Im_number != 0. ||
        temp->next->next->next->next->Im_number != 0.)
    {
        error = true;
        rprin ("Нахождение наибольшего из комплексных чисел не имеет смысла!");
        return;
    }
    temp->number = MAX (temp->next->next->number,
                   temp->next->next->next->next->number);
}
//=============================================================
void Interpreter::min_ ()
{
    if (temp->next->next->Im_number != 0. ||
        temp->next->next->next->next->Im_number != 0.)
    {
        error = true;
        rprin ("Нахождение наименьшего из комплексных чисел не имеет смысла!");
        return;
    }
    temp->number = MIN (temp->next->next->number,
                   temp->next->next->next->next->number);
}
//=============================================================
void Interpreter::nod ()
{
    if (temp->next->next->Im_number != 0. ||
        temp->next->next->next->next->Im_number != 0.)
    {
        error = true;
        rprin ("Нахождение НОД комплексных чисел не имеет смысла!");
        return;
    }
    temp->number = NOD (temp->next->next->number,
                   temp->next->next->next->next->number);
}
//=============================================================
void Interpreter::pow_ ()
{
    complex compl2, res;
    temp_complex_number.Re = temp->next->next->number;
    temp_complex_number.Im = temp->next->next->Im_number;
    compl2.Re = temp->next->next->next->next->number;
    compl2.Im = temp->next->next->next->next->Im_number;
    temp_complex_number.pow_ (temp_complex_number, compl2, &res);
    temp->number = res.Re;
    temp->Im_number = res.Im;
}

//===============================================================================================================
// МЕТОДЫ ВЫЧИСЛЕНИЯ ИНТЕГРАЛА:

// Метод возвращает истину если текущая лексема является функцией вычисления интеграла
bool Interpreter::IsIntegral ()
{
    if (temp->next &&
        temp->next->next &&
        temp->next->next->next &&
        temp->next->next->next->next &&
        temp->next->next->next->next->next &&
        temp->next->next->next->next->next->next &&
        temp->next->next->next->next->next->next->next &&
        temp->next->next->next->next->next->next->next->next &&
        temp->next->next->next->next->next->next->next->next->next) // если справа есть 7 лексем
        if (temp->next->ThisIs == BRECKET_OPEN && // открывающаяся скобка
            (temp->next->next->ThisIs == STRING) && // в строке описана подинтегральная функция
            temp->next->next->next->ThisIs == COMMA && // запятая
            temp->next->next->next->next->ThisIs == STRING && // имя переменной, по которой интегрируем
            temp->next->next->next->next->next->ThisIs == COMMA && // запятая
            IsNumberOrVariable (temp->next->next->next->next->next->next) && // число - нижний предел
            temp->next->next->next->next->next->next->next->ThisIs == COMMA && // запятая
            IsNumberOrVariable (temp->next->next->next->next->next->next->next->next) && // число - верхний предел
            temp->next->next->next->next->next->next->next->next->next->ThisIs == BRECKET_CLOSE) // закрывающаяся скобка
        {
            return true;
        }
    return false;
}
//=============================================================
// Вычисление определенного интеграла методом Симпсона.
// integral ("интегрируемая_функция", подинтегральная_переменная, верхний_предел, нижний_предел);
//
// Первый аргумент - строка - содержит интегрируемую функцию - т.е. отдельное
// арифметическое выражение, поэтому для него создается отдельный объект Integral класса
// Interpreter, в него передаются массивы пользовательских функций, переменных и массивов
// и вызывается метод interpret. Интегрировать можно только по переменной, имя которой
// указано во втором параметре функции, для чего для Integral создается эта переменная.
// В ходе работы метода Integral.interpret могут еще вычисляться интегралы, создаваться
// пользовательские функции, переменные и массивы, область их видимости - объект Integral,
// но могут измениться значения переменных текущего объекта. Функции не переопределяются,
// массивы также могут изменить содержание.
void Interpreter::integral ()
{
    if (!IsIntegral ()) // если не функция integral
        return;

    // указатель на подинтегральную переменную
    char *ptrNameVar = strings[temp->next->next->next->next->IndexString];

    // если первая буква имени не буква и не символ _
    if (!isalpha_(*ptrNameVar) && *ptrNameVar != '_')
    {
        rprin ("\rНеправильно указано имя подинтегральной переменной!");
        error = true;
        return;
    }

    // дойти до конца имени и обозначить его конец нулевым символом, так как в имени
    // может быть указано несколько слов
    while (*ptrNameVar &&
        (isalpha_(*ptrNameVar) ||
        isdigit (*ptrNameVar ||
        *ptrNameVar == '_')))
    {
        ++ptrNameVar;
    }
    *ptrNameVar = 0;

    // установить указатель на начало имени
    ptrNameVar = strings[temp->next->next->next->next->IndexString];

    // если считанное имя - ключевое, зарезервированное
    if (EqualNamesUnique (ptrNameVar))
    {
        error = true;
        rprin (ptrNameVar);
        rprin (" - ключевое слово, неприменимое для имени переменной!");
        return;
    }

    // если есть пользовательская функция с таким именем
    if (ExistsFunction (ptrNameVar))
    {
        error = true;
        rprin (ptrNameVar);
        rprin (" - имя функции, неприменимое для имени переменной!");
        return;
    }

    // если есть пользовательский массив с таким именем
    if (ExistsArray(ptrNameVar))
    {
        error = true;
        rprin (ptrNameVar);
        rprin (" - имя массива, неприменимое для имени переменной!");
        return;
    }

    Interpreter *Integral; // объект для разбора строки с подинтегральной функцией
    Integral = new Interpreter;

    double a = temp->next->next->next->next->next->next->number, // нижний предел интеграла
           b = temp->next->next->next->next->next->next->next->next->number; // верхний предел

    // сделать видимыми в объекте Integral все пользовательские переменные текущего объекта
    Integral->countVariables = this->countVariables;
    // сделать видимыми в объекте Integral все пользовательские функции текущего объекта
    Integral->countFunctions = this->countFunctions;
    // сделать видимыми в объекте Integral все пользовательские массивы текущего объекта
    Integral->countArray = this->countArray;
    // передать в объект счетчик строк
    Integral->countString = this->countString;
    // создать переменную x. Подинтегральную функцию интегрируем по x.
    Integral->CreateVariable (ptrNameVar);

    const double e = 1e-5; // точность вычисления интеграла
    double s = 0.0, w, x, h, n = 10.0;

    // на цикл вычисления интеграла отводится строго определенное время, поэтому необходимо
    // засечь момент начала интегрирования.
    time_t start = time (NULL), // начало засекли
           end; // время окончания интегрирования

    complex current; // текущее значение интеграла

    do // цикл вычисления интеграла
    {
        w = s; h = (b-a)/n;

        for (s = 0.0, x = a; x < b - h; x += 2.0*h)
        {
            // инициализировать переменную x объекта Integral: x.Re = x(переменная double,
            // объявлена выше), x.Im = 0
            Integral->ChangeValueVariable (ptrNameVar, x, 0.);

            // вычислить подинтегральное выражение.
            current = Integral->interpret (strings[temp->next->next->IndexString]);
            // удалить из глобального массива strings только что созданные строки
            DeleteStrings (this->countString);
            // на место удаленных строк можно записывать новые
            Integral->countString = this->countString;
            if (error       ||
                ThisComment ||
                return_func) // если нашлась ошибка или пустое выражение или встретилась
                // функция return, то завершить функцию
            {
                // Удалить из глобального массива ListArray массивы объявленные объектом Integral
                DelArrays (countArray, Integral->countArray);
                // Удалить из глобального массива ListFunc функции объявленные объектом Integral
                DelFunc (countFunctions, Integral->countFunctions);
                delete Integral;
                return;
            }

            s += current.Re;

            // если цикл вычисления интеграла занимает слишком много времени, то
            // завершить интегрирование с выводом предупреждения на экран, результат
            // вернуть в список лексем какой получился
            if (difftime (end=time(NULL), start) == TIME_CALCULATION_INTEGRALE)
            {
                rputs ("Не удалось точно вычислить интеграл,\
 ваш компьютер не рассчитан на такие \nгромоздкие вычисления.\
 Попробуйте уменьшить интервал интегрирования и/или \nупростить подинтегральную функцию.");
                goto end_calculation; // завершить интегрирование
            }

            // изменить значение переменной x
            Integral->ChangeValueVariable (ptrNameVar, x+h, 0.);
            current = Integral->interpret (strings[temp->next->next->IndexString]);
            // удалить из глобального массива strings только что созданные строки
            DeleteStrings (this->countString);
            Integral->countString = this->countString;
            if (error || ThisComment || return_func)
            {
                // Удалить из глобального массива ListArray массивы объявленные объектом Integral
                DelArrays (countArray, Integral->countArray);
                // Удалить из глобального массива ListFunc функции объявленные объектом Integral
                DelFunc (countFunctions, Integral->countFunctions);
                delete Integral;
                return;
            }

            s += 4.*current.Re;

            // изменить значение переменной x
            Integral->ChangeValueVariable (ptrNameVar, x+2.*h, 0.);
            current = Integral->interpret(strings[temp->next->next->IndexString]);
            // удалить из глобального массива strings только что созданные строки
            DeleteStrings (this->countString);
            Integral->countString = this->countString;
            if (error || ThisComment || return_func)
            {
                // Удалить из глобального массива ListArray массивы объявленные объектом Integral
                DelArrays (countArray, Integral->countArray);
                // Удалить из глобального массива ListFunc функции объявленные объектом Integral
                DelFunc (countFunctions, Integral->countFunctions);
                delete Integral;
                return;
            }

            s += current.Re;
        }
        s *= h/3.0;
        n *= 2.0;
    }
    while (fabs (s-w) > e); // интегрируем пока не достигнем необходимой точности

end_calculation:;
    // В результате интегрирования переменные и массивы текущего объекта могли изменить
    // значения, новых функций, переменных и массивов не появилось.
    // Удалить из глобального массива ListArray массивы объявленные объектом Integral
    DelArrays (countArray, Integral->countArray);
    // Удалить из глобального массива ListFunc функции объявленные объектом Integral
    DelFunc (countFunctions, Integral->countFunctions);
    // переменные объявленные объектом Integral автоматически стали не видны в текущем
    // объекте.
    // удалить из глобального массива strings только что созданные строки
    DeleteStrings (this->countString);
    temp->number = s; // сохранить результат интегрирования
    temp->Im_number = 0.; // с комплесными числами не интегрируем
    delete Integral;
    // удалить из списка лексем ("интегрируемая_функция", подинтегральная_переменная, верхний_предел, нижний_предел)
    Del_2_Elements ();
    Del_2_Elements ();
    Del_2_Elements ();
    Del_2_Elements ();
    DelElementMyVersion (temp->next, &(this->start), &(this->end));
    --countToken;
    // в лексему имени функции записан результат вычислений, тип хранимых данных поменять
    // на числовой
    temp->ThisIs = NUMBER;
    RefreshAllUserData (); // обновить пользовательские данные
    temp = this->start;
}

//===============================================================================================================
// МЕТОДЫ РАБОТЫ С ПОЛЬЗОВАТЕЛЬСКИМИ МАССИВАМИ:


// Метод возвращает истину если в  находится пользовательский массив с
// именем name, в глобальную переменную count запишется индекс существующего массива.
// Метод применяется при создании новых пользовательских массивов, имена функций,
// массивов и переменных не должны совпадать.
bool Interpreter::ExistsArray (char *name)
{
    for(count = 0; count < countArray; count++) // просмотреть в ListArray массивы
    {
        // если уже создан массив с именем name
        if (!strcmp (ListArray[count].nameArray, name))
        {
            return true;
        }
    }
    return false;
}
//===============================================================
// Метод выполняет команду new для создания пользовательского массива, её формат:
// new (имя_массива, число_элементов) - для одномерного массива
// или
// new (имя_массива, число_строк, число_элементов_в_строке) - для двумерного массива.
//
// Все массивы одного типа и хранят комплексные числа.
// Имя массива может быть в кавычках или без кавычек. В памяти двумерные массивы
// располагаются как одномерные. Если массив уже создан, то память под него выделенная
// освобождается и выделяется новый блок памяти, данные при этом сохраняются (например,
// если массив состоял из 5 элементов и затем переопределился и стал состоять из 10
// элементов, то первые 5 элементов сохраняют прежние значения). Допустимо одномерные
// массивы переоперелять в двумерные и наоборот.
// число_строк и число_элементов_в_строке должны быть от 1 до MAX_SIZE_USER_ARRAY.
// Если число элементов указано как дробное, то дробная часть отбрасывается.
// Метод возвращает в список лексем число элементов созданного массива.
void Interpreter::run_new ()
{
    bool redefinition = false; // флаг определяет, переопределяется ли массив

    // одномерный массив
    // если текущая лексема - команда new, затем справа открывающаяся скобка, строка - имя
    // массива, запятая, численное значение - размер массива, закрывающаяся скобка
    if (temp->ThisIs == NEW &&
        temp->next &&
        temp->next->next &&
        temp->next->next->next &&
        temp->next->next->next->next &&
        temp->next->next->next->next->next &&
        temp->next->ThisIs == BRECKET_OPEN &&
        temp->next->next->ThisIs == STRING &&
        temp->next->next->next->ThisIs == COMMA &&
        IsNumberOrVariable (temp->next->next->next->next)  &&
        temp->next->next->next->next->next->ThisIs == BRECKET_CLOSE)
    {
        // если имя массива - ключевое слово
        if (EqualNamesUnique (strings [temp->next->next->IndexString]))
        {
            error = true;
            rprin (strings [temp->next->next->IndexString]);
            rprin (" - ключевое слово, неприменимое для имени массива!");
            return;
        }

        // если имя массива - имя функции
        if (ExistsFunction (strings [temp->next->next->IndexString]))
        {
            rprin(strings [temp->next->next->IndexString]);
            rprin(" - имя функции, не применимое для имени массива");
            error = true;
            return;
        }

        // если имя массива - имя переменной
        if (ExistsVariable (strings [temp->next->next->IndexString]))
        {
            rprin(strings [temp->next->next->IndexString]);
            rprin(" - имя переменной, не применимое для имени массива");
            error = true;
            return;
        }

        // если размер массива указан < 1 или больше допустимого
        if ((int)temp->next->next->next->next->number < 1 ||
            (int)temp->next->next->next->next->number > MAX_SIZE_USER_ARRAY)
        {
            rprin ("Число элементов в массиве ");
            rprin (strings[temp->next->next->IndexString]);
            rprin (" должно быть от 1 до ");
            printf ("%i", MAX_SIZE_USER_ARRAY);
            error = true;
            return;
        }

        // если переопределение массива, размерность dim в методе FillingListLexeme
        // указывается как > 0
        if (ListArray[temp->next->next->IndexArray].dim > 0)
        {
            // нужно скопировать содержимое массива в массив box с размером указанным
            // в параметрах функции new, удалить массив, переопределить указатель на
            // массив box. Таким образом переопределяя массив данные можно не терять,
            // изменяя всего лишь размер/размерность массива
            complex *box;
            box = new complex [(int)temp->next->next->next->next->number];
            if (!box) // если не удалось выделить память
            {
                rprin ("Не удалось измененить размер массива ");
                rprin (strings[temp->next->next->IndexString]);
                error = true;
                return;
            }

            // заполнить box данными из массива прежнего размера
            int min = ((ListArray[temp->next->next->IndexArray].dim == 2) ? 0 : 1);
            for (count=0;
                 count < (int)temp->next->next->next->next->number;
                 box[count/*-min*/] = ListArray[temp->next->next->IndexArray].elements[count++]);

            // удалить массив  прежнего размера
            delete[]ListArray[temp->next->next->IndexArray].elements;
            // переопределить указатель на массив нового размера.
            ListArray[temp->next->next->IndexArray].elements = box;
            redefinition = true;
        }

        // определить размерность массива
        ListArray[temp->next->next->IndexArray].dim = 1; // 1 - одномерный
        // определить число строк массива равное 1, т.к. одномерный
        ListArray[temp->next->next->IndexArray].i = 1;
        // определить число элементов в строке
        ListArray[temp->next->next->IndexArray].j = (int)temp->next->next->next->next->number;

        // если массив не переопределялся
        if (!redefinition)
        {
            // скопировать имя массива в ListArray
            strcpy (ListArray[temp->next->next->IndexArray].nameArray, strings[temp->next->next->IndexString]);
            // выделить память под новый массив
            ListArray[temp->next->next->IndexArray].elements = new complex [(int)temp->next->next->next->next->number];
            if (!ListArray[temp->next->next->IndexArray].elements)
            {
                rprin ("Не удалось создать массив ");
                rprin (ListArray[temp->next->next->IndexArray].nameArray);
                error = true;
                return;
            }
            rprin("Объявлен массив ");
            rprin (ListArray[temp->next->next->IndexArray].nameArray);
            printf (" [%i]\n", ListArray[temp->next->next->IndexArray].j);
        }

        // если массив был переопределен
        if (redefinition )
        {
            // если массив был переопределен вне цикла, вывести сообщение на консоль
            if (!count_loop)
            {
                rprin ("Переопределен массив ");
                rprin (ListArray[temp->next->next->IndexArray].nameArray);
                printf (" [%i]\n", ListArray[temp->next->next->IndexArray].j);
            }
        }

        // на место лексемы new запишется размер нового массива
        temp->ThisIs = NUMBER; // переопределить тип лексемы new -> численный
        temp->Im_number = 0;
        temp->number = ListArray[temp->next->next->IndexArray].j; // записать размер массива
        Del_2_Elements (); // удалить из списка лексем открывающуюся скобку и имя массива
        Del_2_Elements (); // удалить запятую и размер массива
        DelElementMyVersion (temp->next, &start, &end);
        --countToken; // удалить закрывающуюся скобку
        temp = start; // обязательно перейти к началу списка лексем
        return;
    } // одномерный массив

    // двумерный массив
    // если текущая лексема - команда new, затем справа открывающаяся скобка, строка - имя
    // массива, запятая, численное значение - количество строк массива, запятая, численное
    // значение - количество столбцов массива, закрывающаяся скобка
    if (temp->ThisIs == NEW &&
        temp->next &&
        temp->next->next &&
        temp->next->next->next &&
        temp->next->next->next->next &&
        temp->next->next->next->next->next &&
        temp->next->next->next->next->next->next &&
        temp->next->next->next->next->next->next->next &&
        temp->next->ThisIs == BRECKET_OPEN &&
        temp->next->next->ThisIs == STRING &&
        temp->next->next->next->ThisIs == COMMA &&
        IsNumberOrVariable (temp->next->next->next->next)  &&
        temp->next->next->next->next->next->ThisIs == COMMA &&
        IsNumberOrVariable (temp->next->next->next->next->next->next)  &&
        temp->next->next->next->next->next->next->next->ThisIs == BRECKET_CLOSE)
    {printf("\nfffffffffffff\n");
        // если имя массива - ключевое слово
        if (EqualNamesUnique (strings [temp->next->next->IndexString]))
        {
            error = true;
            rprin (strings [temp->next->next->IndexString]);
            rprin (" - ключевое слово, неприменимое для имени массива!");
            return;
        }

        // если имя массива - имя функции
        if (ExistsFunction (strings [temp->next->next->IndexString]))
        {
            rprin(strings [temp->next->next->IndexString]);
            rprin(" - имя функции, не применимое для имени массива");
            error = true;
            return;
        }

        // если имя массива - имя переменной
        if (ExistsVariable (strings [temp->next->next->IndexString]))
        {
            rprin(strings [temp->next->next->IndexString]);
            rprin(" - имя переменной, не применимое для имени массива");
            error = true;
            return;
        }

        // вычислить общее число элементов в двумерном массиве, равное числу строк * на
        // число столбцов
        int dim = (int)temp->next->next->next->next->number * (int)temp->next->next->next->next->next->next->number;
        dim++; // для двумерного массива число элементов должно быть на 1 больше чтобы не
        // было проблем с памятью

        if (dim < 1 ||
            dim > MAX_SIZE_USER_ARRAY) // если недопустимый размер массива
        {
            rprin ("Число элементов в массиве ");
            rprin (strings[temp->next->next->IndexString]);
            rprin (" должно быть от 1 до ");
            printf ("%i", MAX_SIZE_USER_ARRAY);
            error = true;
            return;
        }

        // если переопределение массива, размерность dim в методе FillingListLexeme
        // указывается как > 0
        if (ListArray[temp->next->next->IndexArray].dim > 0) // переопределение массива
        {
            // нужно скопировать содержимое массива в массив box размером равным dim,
            // удалить массив, переопределить указатель на массив box. Таким образом
            // переопределяя массив данные можно не терять, изменяя всего лишь
            // размер/размерность массива
            complex *box;
            box = new complex [dim];
            if (!box) // если не удалось выделить память
            {
                rprin ("Не удалось измененить размер массива ");
                rprin (strings[temp->next->next->IndexString]);
                error = true;
                return;
            }
            // заполнить box данными из массива прежнего размера
            for (count=0;
                 count < (int)temp->next->next->next->next->next->next->number;
                 box[count] = ListArray[temp->next->next->IndexArray].elements[count++]);

            // удалить массив  прежнего размера
            delete[]ListArray[temp->next->next->IndexArray].elements;
            // переопределить указатель на массив нового размера.
            ListArray[temp->next->next->IndexArray].elements = box;
            redefinition = true;
            rprin ("Переопределен массив ");
        }

        // определить размерность массива
        ListArray[temp->next->next->IndexArray].dim = 2;
        // записать число строк массива
        ListArray[temp->next->next->IndexArray].i = (int)temp->next->next->next->next->number;
        // записать число столбцов массива
        ListArray[temp->next->next->IndexArray].j = (int)temp->next->next->next->next->next->next->number;

        // если массив не переопределялся
        if (!redefinition)
        {
            // скопировать имя массива в ListArray
            strcpy (ListArray[temp->next->next->IndexArray].nameArray, strings[temp->next->next->IndexString]);
            // выделить память под новый массив
            ListArray[temp->next->next->IndexArray].elements = new complex [dim];
            if (!ListArray[temp->next->next->IndexArray].elements)
            {
                rprin ("Не удалось создать массив ");
                rprin (ListArray[temp->next->next->IndexArray].nameArray);
                error = true;
                return;
            }
            rprin("Объявлен массив ");
        }

        // вывести имя массива на консоль
        rprin (ListArray[temp->next->next->IndexArray].nameArray);
        // вывести размер массива на консоль
        printf (" [%i][%i]\n", ListArray[temp->next->next->IndexArray].i, ListArray[temp->next->next->IndexArray].j);

        // на место лексемы new запишется размер нового массива
        temp->ThisIs = NUMBER; // переопределить тип лексемы new -> численный
        temp->Im_number = 0;
        temp->number = dim;
        Del_2_Elements (); // удалить из списка лексем открывающуюся скобку и имя массива
        Del_2_Elements (); // удалить запятую и число строк
        Del_2_Elements (); // удалить запятую и число столбцов
        DelElementMyVersion (temp->next, &start, &end);
        --countToken; // удалить закрывающуюся скобку
        temp = start;
    }// одномерный массив
}
//===============================================================
// Метод переводит элемент одномерного массива в число:
// имя_массива [индекс] -> число
// имя_массива [индекс].re -> getre (число)
// Индексация массива начинается с нуля.
void Interpreter::element_array_1D_to_number ()
{
    // если текущая лексема - элемент  массива и справа есть три лексемы
    if (temp->ThisIs == ELEMENT_ARRAY &&
        temp->next && temp->next->next && temp->next->next->next)
    {
        // если одномерный массив
        if (ListArray [temp->IndexArray].dim == 1)
        {
            // если справа от текущей лексемы открывающаяся квадратная скобка, численное
            // значение и закрывающаяся квадратная скобка: a[index]
            if (temp->next->ThisIs == SQUARE_BRECKET_OPEN &&
                IsNumberOrVariable (temp->next->next) &&
                temp->next->next->next->ThisIs == SQUARE_BRECKET_CLOSE)
            {
                // если после закрывающейся квадратной скобки оператор присваивания или
                // открывающаяся квадратная скобка: a[index]= или a[index][
                if (temp->next->next->next->next &&
                    (temp->next->next->next->next->ThisIs == EQUAL ||
                     temp->next->next->next->next->ThisIs == SQUARE_BRECKET_OPEN))
                {
                    return;
                }

                // записать в текущую лексему индекс элемента массива
                temp->j = (int)temp->next->next->number;

                // если индекс отрицательный или больше или равно размеру массива
                if (temp->j < 0 || temp->j >= ListArray [temp->IndexArray].j)
                {
                    rprin ("Выход за пределы массива ");
                    rprin (ListArray [temp->IndexArray].nameArray);
                    error = true;
                    return;
                }

                // записать в текущую лексему комплексную и действительную часть элемента
                // массива
                temp->number = ListArray[temp->IndexArray].elements[temp->j].Re;
                temp->Im_number = ListArray[temp->IndexArray].elements[temp->j].Im;

                // если после закрывающейся квадратной скобки есть точка и квалификатор
                // re или im: a[index].re
                if (temp->next->next->next->next &&
                    temp->next->next->next->next->ThisIs == POINT &&
                    temp->next->next->next->next->next &&
                    (temp->next->next->next->next->next->ThisIs == MEMBER_STRUCT_RE ||
                    temp->next->next->next->next->next->ThisIs == MEMBER_STRUCT_IM))
                {
                    // если после квалификатора оператор присваивания или открывающаяся
                    // квадратная скобка: a[index].re=
                    if (temp->next->next->next->next->next->next &&
                        (temp->next->next->next->next->next->next->ThisIs == EQUAL ||
                        temp->next->next->next->next->next->next->ThisIs == SQUARE_BRECKET_OPEN))
                    {
                        return;
                    }

                    // Преобразовать: имя_массива [индекс].re -> getre (число)
                    // определить текущую лексему как число
                    temp->ThisIs = NUMBER;
                    // лексему с индексом определить как число
                    temp->next->next->ThisIs = temp->ThisIs;
                    // в лексему с индексом скопировать индекс элемента массива
                    temp->next->next->j = temp->j;
                    // в лексему с индексом скопировать индекс массива
                    temp->next->next->IndexArray = temp->IndexArray;
                    // Преобразовать: имя_массива [индекс].re -> getre (число)
                    if (temp->next->next->next->next->next->ThisIs == MEMBER_STRUCT_RE)
                    {
                        temp->next->next->number = temp->number;
                        temp->next->next->Im_number = 0;
                        temp->ThisIs = GETRE;
                    }
                    else
                    {
                    // Преобразовать: имя_массива [индекс].im -> getim (число)
                        temp->next->next->Im_number = temp->Im_number;
                        temp->next->next->number = 0;
                        temp->ThisIs = GETIM;
                    }
                    // квадратные скобки поменять на круглые
                    temp->next->ThisIs = BRECKET_OPEN;
                    temp->next->next->next->ThisIs = BRECKET_CLOSE;
                    // удалить из списка лексем точку и квалификатор re
                    DelElementMyVersion (temp->next->next->next->next, &start, &end);
                    DelElementMyVersion (temp->next->next->next->next, &start, &end);
                    countToken -= 2;
                    temp = start;
                    return;
                }

                // иначе удалить квадратные скобки и индекс
                temp->ThisIs = NUMBER;
                Del_2_Elements ();
                DelElementMyVersion (temp->next, &start, &end);
                --countToken;
                temp = start;
            } // if (temp->next->ThisIs == SQUARE_BRECKET_OPEN &&
        } // if (ListArray [temp->IndexArray].dim == 1) // одномерный массив
    }// if (temp->ThisIs == ELEMENT_ARRAY &&...
}
//===============================================================
// Метод переводит элемент двумерного массива в число:
// имя_массива [индекс_строки][индекс_столбца] -> число
// имя_массива [индекс_строки][индекс_столбца].re -> getre (число)
void Interpreter::element_array_2D_to_number ()
{
    // если текущая лексема - элемент  массива и справа есть 6 лексем
    if (temp->ThisIs == ELEMENT_ARRAY &&
        temp->next &&
        temp->next->next &&
        temp->next->next->next &&
        temp->next->next->next->next &&
        temp->next->next->next->next->next &&
        temp->next->next->next->next->next->next)
    {
        if (ListArray [temp->IndexArray].dim == 2) // двумерный массив
        {
            // если справа от текущей лексемы
            // открывающаяся квадратная скобка, число, закрывающаяся квадратная скобка,
            // открывающаяся квадратная скобка, число, закрывающаяся квадратная скобка:
            // a[index1][index2]
            if (temp->next->ThisIs == SQUARE_BRECKET_OPEN &&
                IsNumberOrVariable (temp->next->next) &&
                temp->next->next->next->ThisIs == SQUARE_BRECKET_CLOSE &&
                temp->next->next->next->next->ThisIs == SQUARE_BRECKET_OPEN &&
                IsNumberOrVariable (temp->next->next->next->next->next) &&
                temp->next->next->next->next->next->next->ThisIs == SQUARE_BRECKET_CLOSE)
            {
                // если после закрывающейся квадратной скобки оператор присваивания или
                // открывающаяся квадратная скобка: a[index1][index2] =
                // или a[index1][index2][
                if (temp->next->next->next->next->next->next->next &&
                    (temp->next->next->next->next->next->next->next->ThisIs == EQUAL
                    ||temp->next->next->next->next->next->next->next->ThisIs ==  SQUARE_BRECKET_OPEN) )
                {
                    return;
                }

                // в памяти двумерные массивы распалагаются как одномерные, поэтому в
                // temp->j занести индекс массива равный индекс_строки*число_столбцов+индекс_столбца
                temp->j = ((int)temp->next->next->number)*(ListArray [temp->IndexArray].j) + ((int)temp->next->next->next->next->next->number);

                // если выход за пределы массива
                if ((int)temp->next->next->number < 0 ||
                    (int)temp->next->next->number >= ListArray [temp->IndexArray].i ||
                    (int)temp->next->next->next->next->next->number < 0 ||
                    (int)temp->next->next->next->next->next->number >= ListArray [temp->IndexArray].j)
                {
                    rprin ("Выход за пределы массива ");
                    rprin (ListArray [temp->IndexArray].nameArray);
                    error = true;
                    return;
                }

                // записать в текущую лексему комплексную и действительную часть элемента
                // массива
                temp->number = ListArray[temp->IndexArray].elements[temp->j].Re;
                temp->Im_number = ListArray[temp->IndexArray].elements[temp->j].Im;

                // если а[index1][index2].re или а[index1][index2].im
                if (temp->next->next->next->next->next->next->next &&
                    temp->next->next->next->next->next->next->next->ThisIs == POINT &&
                    temp->next->next->next->next->next->next->next->next &&
                    (temp->next->next->next->next->next->next->next->next->ThisIs == MEMBER_STRUCT_RE ||
                    temp->next->next->next->next->next->next->next->next->ThisIs == MEMBER_STRUCT_IM))
                {
                    // если а[index1][index2].re =
                    if (temp->next->next->next->next->next->next->next->next->next &&
                        temp->next->next->next->next->next->next->next->next->next->ThisIs == EQUAL)
                    {
                        return;
                    }

                    // Преобразовать: имя_массива [индекс1][индекс2].re -> getre (число)
                    // определить текущую лексему как число
                    temp->ThisIs = NUMBER;
                    // лексему с индексом определить как число
                    temp->next->next->ThisIs = temp->ThisIs;
                    // в лексему с индексом скопировать индекс элемента массива
                    temp->next->next->j = temp->j;
                    // в лексему с индексом скопировать индекс массива
                    temp->next->next->IndexArray = temp->IndexArray;

                    // Преобразовать: имя_массива [индекс1][индекс2].re -> getre (число)
                    if (temp->next->next->next->next->next->next->next->next->ThisIs == MEMBER_STRUCT_RE)
                    {
                        temp->next->next->number = temp->number;
                        temp->ThisIs = GETRE;
                    }
                    else
                    {
                    // Преобразовать: имя_массива [индекс1][индекс2].im -> getim (число)
                        temp->next->next->Im_number = temp->Im_number;
                        temp->ThisIs = GETIM;
                    }
                    // квадратные скобки поменять на круглые
                    temp->next->ThisIs = BRECKET_OPEN;
                    temp->next->next->next->ThisIs = BRECKET_CLOSE;
                    // удалить 5 лексем после )
                    DelElementMyVersion (temp->next->next->next->next, &start, &end);
                    DelElementMyVersion (temp->next->next->next->next, &start, &end);
                    DelElementMyVersion (temp->next->next->next->next, &start, &end);
                    DelElementMyVersion (temp->next->next->next->next, &start, &end);
                    DelElementMyVersion (temp->next->next->next->next, &start, &end);
                    countToken -= 5;
                    temp = start;
                    return;
                }

                // иначе удалить квадратные скобки и индексы
                temp->ThisIs = NUMBER;
                Del_2_Elements ();
                Del_2_Elements ();
                Del_2_Elements ();
                temp = start;
            }
        } // if (ListArray [temp->IndexArray].dim == 1) // двумерный массив
    }// if (temp->ThisIs == ELEMENT_ARRAY &&...
}
//===============================================================
// Инициализация элемента одномерного массива:
// a[index] = число
// a[index].re = число
// В список лексем возвращается число
void Interpreter::initialization_element_array_1D ()
{
    // если текущая лексема - элемент  массива и справа есть индекс в квадратных скобках,
    // оператор равно, число и после числа нет лексем или есть запятая или )
    if (temp->ThisIs == ELEMENT_ARRAY &&
        temp->next &&
        temp->next->ThisIs == SQUARE_BRECKET_OPEN &&
        temp->next->next &&
        IsNumberOrVariable (temp->next->next) &&
        temp->next->next->next &&
        temp->next->next->next->ThisIs == SQUARE_BRECKET_CLOSE &&
        temp->next->next->next->next &&
        temp->next->next->next->next->ThisIs == EQUAL &&
        temp->next->next->next->next->next &&
        IsNumberOrVariable (temp->next->next->next->next->next) &&
        (!temp->next->next->next->next->next->next || (temp->next->next->next->next->next->next->ThisIs == COMMA || temp->next->next->next->next->next->next->ThisIs == BRECKET_CLOSE)))
    {
        // если массив указан как двумерный
        if (ListArray [temp->IndexArray].dim == 2)
        {
            rprin ("Неправильное обращение к элементу двумерного массива ");
            rputs (ListArray [temp->IndexArray].nameArray);
            error = true;
            return;
        }

        // записать в текущую лексему индекс элемента массива
        temp->j = (int)temp->next->next->number;

        // если индекс отрицательный или больше или равно размеру массива
        if (temp->j < 0 || temp->j >= ListArray [temp->IndexArray].j)
        {
            rprin ("Выход за пределы массива ");
            rprin (ListArray [temp->IndexArray].nameArray);
            error = true;
            return;
        }

        // определить текущую лексему как число
        temp->ThisIs = NUMBER;
        // записать в текущую лексему комплексную и действительную часть элемента массива
        temp->number = ListArray [temp->IndexArray].elements[temp->j].Re = temp->next->next->next->next->next->number;
        temp->Im_number = ListArray [temp->IndexArray].elements[temp->j].Im = temp->next->next->next->next->next->Im_number;
        // обновить значение элемента массива: в списке лексем найти все нужные элементы
        // массива и инициализировать их комплексную и действительную часть
        RefreshValueElementArrayInList (ListArray [temp->IndexArray].elements[temp->j].Re, ListArray [temp->IndexArray].elements[temp->j].Im);
        // удалить из списка лексем индекс, квадратные скобки, = и число
        Del_2_Elements ();
        Del_2_Elements ();
        DelElementMyVersion (temp->next, &start, &end);
        --countToken;
        temp = start;
        return;
    } // инициализация элемента одномерного массива

    // инициализация комплексной или действительной части элемента одномерного массива.
    // если текущая лексема - элемент  массива и справа есть индекс в квадратных скобках,
    // квалификатор re или im, оператор равно, число и после числа нет лексем или есть
    // запятая или )
    if (temp->ThisIs == ELEMENT_ARRAY &&
        temp->next &&
        temp->next->ThisIs == SQUARE_BRECKET_OPEN &&
        temp->next->next &&
        IsNumberOrVariable (temp->next->next) &&
        temp->next->next->next &&
        temp->next->next->next->ThisIs == SQUARE_BRECKET_CLOSE &&
        temp->next->next->next->next &&
        temp->next->next->next->next->ThisIs == POINT &&
        temp->next->next->next->next->next &&
        IsMemberStruct (temp->next->next->next->next->next) &&
        temp->next->next->next->next->next->next &&
        temp->next->next->next->next->next->next->ThisIs == EQUAL &&
        temp->next->next->next->next->next->next->next &&
        IsNumberOrVariable (temp->next->next->next->next->next->next->next) &&
        (!temp->next->next->next->next->next->next->next->next || (temp->next->next->next->next->next->next->next->next->ThisIs == COMMA || temp->next->next->next->next->next->next->next->next->ThisIs == BRECKET_CLOSE)))
    {
        // если массив указан как двумерный
        if (ListArray [temp->IndexArray].dim == 2)
        {
            rprin ("Неправильное обращение к элементу двумерного массива ");
            rputs (ListArray [temp->IndexArray].nameArray);
            error = true;
            return;
        }

        // записать в текущую лексему индекс элемента массива
        temp->j = (int)temp->next->next->number;
        // если индекс отрицательный или больше или равно размеру массива
        if (temp->j < 0 || temp->j >= ListArray [temp->IndexArray].j)
        {
            rprin ("Выход за пределы массива ");
            rprin (ListArray [temp->IndexArray].nameArray);
            error = true;
            return;
        }
        // определить текущую лексему как число
        temp->ThisIs = NUMBER;

        // если а[index].re
        if (temp->next->next->next->next->next->ThisIs == MEMBER_STRUCT_RE)
        {
            temp->number = ListArray [temp->IndexArray].elements[temp->j].Re = temp->next->next->next->next->next->next->next->number;
        }
        else // если а[index].im
        {
            temp->Im_number = ListArray [temp->IndexArray].elements[temp->j].Im = temp->next->next->next->next->next->next->next->Im_number;
        }

        // обновить значение элемента массива: в списке лексем найти все нужные элементы
        // массива и инициализировать их комплексную и действительную часть
        RefreshValueElementArrayInList (temp->number, temp->Im_number);
        // удалить из списка лексем индекс, квадратные скобки, точку, идентификатор re или
        // im, = и число
        Del_2_Elements ();
        Del_2_Elements ();
        Del_2_Elements ();
        DelElementMyVersion (temp->next, &start, &end);
        --countToken;
        temp = start;
        return;
    } // инициализация элемента одномерного массива
}
//===============================================================
// Инициализация элемента двумерного массива:
// a[index1][index2] = число
// a[index1][index2].re = число
// В список лексем возвращается число
void Interpreter::initialization_element_array_2D ()
{
    // если справа от текущей лексемы
    // открывающаяся квадратная скобка, число, закрывающаяся квадратная скобка,
    // открывающаяся квадратная скобка, число, закрывающаяся квадратная скобка,
    // оператор равно, число и после числа нет лексем или есть запятая или )
    if (temp->ThisIs == ELEMENT_ARRAY &&
        temp->next &&
        temp->next->ThisIs == SQUARE_BRECKET_OPEN &&
        temp->next->next &&
        IsNumberOrVariable (temp->next->next) &&
        temp->next->next->next &&
        temp->next->next->next->ThisIs == SQUARE_BRECKET_CLOSE &&
        temp->next->next->next->next &&
        temp->next->next->next->next->ThisIs == SQUARE_BRECKET_OPEN &&
        temp->next->next->next->next->next &&
        IsNumberOrVariable (temp->next->next->next->next->next) &&
        temp->next->next->next->next->next->next &&
        temp->next->next->next->next->next->next->ThisIs == SQUARE_BRECKET_CLOSE &&
        temp->next->next->next->next->next->next->next &&
        temp->next->next->next->next->next->next->next->ThisIs == EQUAL &&
        temp->next->next->next->next->next->next->next->next &&
        IsNumberOrVariable (temp->next->next->next->next->next->next->next->next) &&
        (!temp->next->next->next->next->next->next->next->next->next || (temp->next->next->next->next->next->next->next->next->next->ThisIs == COMMA || temp->next->next->next->next->next->next->next->next->next->ThisIs == BRECKET_CLOSE)))
    {
        // если массив указан как одномерный
        if (ListArray [temp->IndexArray].dim == 1)
        {
            rprin ("Неправильное обращение к элементу одномерного массива ");
            rputs (ListArray [temp->IndexArray].nameArray);
            error = true;
            return;
        }

        // в памяти двумерные массивы распалагаются как одномерные, поэтому в
        // temp->j занести индекс массива равный индекс_строки*число_столбцов+индекс_столбца
        temp->j = ((int)temp->next->next->number)*(ListArray [temp->IndexArray].j) + ((int)temp->next->next->next->next->next->number);

        // если выход за пределы массива
        if ((int)temp->next->next->number < 0 ||
            (int)temp->next->next->number >= ListArray [temp->IndexArray].i ||
            (int)temp->next->next->next->next->next->number < 0 ||
            (int)temp->next->next->next->next->next->number >= ListArray [temp->IndexArray].j)
        {
            rprin ("Выход за пределы массива ");
            rprin (ListArray [temp->IndexArray].nameArray);
            error = true;
            return;
        }

        // определить текущую лексему как число
        temp->ThisIs = NUMBER;
        // записать в текущую лексему комплексную и действительную часть элемента массива
        temp->number = ListArray [temp->IndexArray].elements[temp->j].Re = temp->next->next->next->next->next->next->next->next->number;
        temp->Im_number = ListArray [temp->IndexArray].elements[temp->j].Im = temp->next->next->next->next->next->next->next->next->Im_number;
        // обновить значение элемента массива: в списке лексем найти все нужные элементы
        // массива и инициализировать их комплексную и действительную часть
        RefreshValueElementArrayInList (temp->number, temp->Im_number);
        // удалить из списка лексем индексы, квадратные скобки, = и число
        Del_2_Elements ();
        Del_2_Elements ();
        Del_2_Elements ();
        Del_2_Elements ();
        temp = start;
        return;
    } // инициализация элемента двумерного массива

    // инициализация комплексной или действительной части элемента двумерного массива
    // если а[index1][index2].re = число или а[index1][index2].im = число
    if (temp->ThisIs == ELEMENT_ARRAY &&
        temp->next &&
        temp->next->ThisIs == SQUARE_BRECKET_OPEN &&
        temp->next->next &&
        IsNumberOrVariable (temp->next->next) &&
        temp->next->next->next &&
        temp->next->next->next->ThisIs == SQUARE_BRECKET_CLOSE &&
        temp->next->next->next->next &&
        temp->next->next->next->next->ThisIs == SQUARE_BRECKET_OPEN &&
        temp->next->next->next->next->next &&
        IsNumberOrVariable (temp->next->next->next->next->next) &&
        temp->next->next->next->next->next->next &&
        temp->next->next->next->next->next->next->ThisIs == SQUARE_BRECKET_CLOSE &&
        temp->next->next->next->next->next->next->next &&
        temp->next->next->next->next->next->next->next->ThisIs == POINT &&
        temp->next->next->next->next->next->next->next->next &&
        IsMemberStruct(temp->next->next->next->next->next->next->next->next) &&
        temp->next->next->next->next->next->next->next->next->next &&
        temp->next->next->next->next->next->next->next->next->next->ThisIs == EQUAL &&
        temp->next->next->next->next->next->next->next->next->next->next &&
        IsNumberOrVariable (temp->next->next->next->next->next->next->next->next->next->next) &&
        (!temp->next->next->next->next->next->next->next->next->next->next->next || (temp->next->next->next->next->next->next->next->next->next->next->next->ThisIs == COMMA || temp->next->next->next->next->next->next->next->next->next->next->next->ThisIs == BRECKET_CLOSE)))
    {
        // если массив указан как одномерный
        if (ListArray [temp->IndexArray].dim == 1)
        {
            rprin ("Неправильное обращение к элементу одномерного массива ");
            rputs (ListArray [temp->IndexArray].nameArray);
            error = true;
            return;
        }

        // в памяти двумерные массивы распалагаются как одномерные, поэтому в
        // temp->j занести индекс массива равный индекс_строки*число_столбцов+индекс_столбца
        temp->j = ((int)temp->next->next->number)*(ListArray [temp->IndexArray].j) + ((int)temp->next->next->next->next->next->number);

        // если выход за пределы массива
        if ((int)temp->next->next->number < 0 ||
            (int)temp->next->next->number >= ListArray [temp->IndexArray].i ||
            (int)temp->next->next->next->next->next->number < 0 ||
            (int)temp->next->next->next->next->next->number >= ListArray [temp->IndexArray].j)
        {
            rprin ("Выход за пределы массива ");
            rprin (ListArray [temp->IndexArray].nameArray);
            error = true;
            return;
        }

        // определить текущую лексему как число
        temp->ThisIs = NUMBER;

        // если а[index1][index2].re
        if (temp->next->next->next->next->next->next->next->next->ThisIs == MEMBER_STRUCT_RE)
        {
            temp->number = ListArray [temp->IndexArray].elements[temp->j].Re = temp->next->next->next->next->next->next->next->next->next->next->number;
        }
        else // если а[index1][index2].im
        {
            temp->Im_number = ListArray [temp->IndexArray].elements[temp->j].Im = temp->next->next->next->next->next->next->next->next->next->next->Im_number;
        }

        // обновить значение элемента массива: в списке лексем найти все нужные элементы
        // массива и инициализировать их комплексную и действительную часть
        RefreshValueElementArrayInList (temp->number, temp->Im_number);
        // удалить из списка лексем индексы, квадратные скобки, точку, идентификатор re или
        // im, = и число
        Del_2_Elements ();
        Del_2_Elements ();
        Del_2_Elements ();
        Del_2_Elements ();
        Del_2_Elements ();
        temp = start;
        return;
    } // инициализация элемента одномерного массива
}
//===============================================================
// Метод применяется в случае изменения элемента массива значения на Re_value и Im_value
// (инициализация, присваивание нового значения). На элемент ссылается указатель temp.
// Новое значение заносится в массив пользовательских массивов. Также все элементы
// массива в выражении с соответствующим индексом меняют свои значения.
void Interpreter::RefreshValueElementArrayInList (double Re_value, double Im_value)
{

    class token *ptr = start; // указатель для навигации по списку лексем

    while (ptr) // просмотреть список и найти все указанные элементы массива
    {
        // если нужный элемент в списке найдена
        if (ptr->ThisIs == NUMBER &&
            ptr->IndexArray == temp->IndexArray)
        {
            // обновить  действительное и комплексное значение элемента массива
            ptr->number    = Re_value;
            ptr->Im_number = Im_value;
        }
        ptr = ptr->next; // перейти к следующей лексеме списка
    }
}
//===============================================================
// метод удаляет из ListArray элементы в диапазоне от IndexBegin до IndexEnd
// используется при разграничении области видимости объектов, например после выполнения
// конструкции if необходимо удалить все массивы объявленные внутри неё.
void Interpreter::DelArrays (int IndexBegin, int IndexEnd)
{
    // просмотреть массив пользовательских массивов
    for (; IndexBegin < IndexEnd; ++IndexBegin)
    {
        ListArray[IndexBegin].dim = ListArray[IndexBegin].nameArray[0] = 0;
        delete []ListArray[IndexBegin].elements; // удалить массив
        ListArray[IndexBegin].elements = 0;
    }
}

//===============================================================================================================
// МЕТОДЫ РАБОТЫ С ПОЛЬЗОВАТЕЛЬСКИМИ ПЕРЕМЕННЫМИ:

// Метод используется при создании новых переменных.
// Возвращает истину, если строка str есть в списке имен функций/операторов/констант,
// т.е. не может быть именем переменной.
bool Interpreter::EqualNamesUnique (char *str)
{
    int i = 0;
    for (; *ListNamesUnique[i]; ++i) // просмотреть список имен функций/операторов/констант
    {
            if (!strcmp (ListNamesUnique[i], str)) // строки равны
                return true;
    }
    return false; // строка str не найдена в массиве ListNamesUnique
}
//===============================================================
// Метод применяется в случае изменения переменной значения на Re_value и Im_value
// (инициализация, присваивание нового значения). На переменную ссылается
// указатель temp. Новое значение заносится в массив пользовательских
// переменных. Также все переменные в выражении с данным именем меняют свои значения.
void Interpreter::SaveValue (double Re_value, double Im_value)
{
    // занести в массив переменыых новое значение переменной
    ListVar[temp->IndexVar].ValueVar    = Re_value;
    ListVar[temp->IndexVar].Im_ValueVar = Im_value;

    class token *ptr = start; // указатель для навигации по списку лексем,
// если в выражении несколько переменных с данным именем, то необходимо обновить их значения.

    while (ptr) // просмотреть список и найти все указанные переменные
    {
        if (ptr->ThisIs == VARIABLE &&
            ptr->IndexVar == temp->IndexVar) // если нужная переменная в списке найдена
        {
            ptr->number    = Re_value; // обновить её действительное и комплексное значение
            ptr->Im_number = Im_value;
        }
        ptr = ptr->next; // перейти к следующей лексеме списка
    }
}
//===============================================================
// Метод вызывается при разбиении выражения на лексемы, когда с помощью метода
// EqualNamesUnique выяснилось что данная строка может быть именем переменной.
void Interpreter::AddVariableInList (char *add)
{
    int count = 0;
    for(; count < countVariables; ++count) // просмотреть список переменных
    {
        // если в массиве ListVar есть переменная с именем add, то новой лексеме присвоить
        // значение существующей переменной, IndexVar ссылается на эту переменную
        // в массиве ListVar
        if (!strcmp (ListVar[count].NameVar, add))
        {
            temp->number   = ListVar[count].ValueVar;
            temp->Im_number   = ListVar[count].Im_ValueVar;
            temp->IndexVar = count;
            return;
        }
    }

    // иначе необходимо занести в список новую переменную:
    if (count == QUANTITY_VARIABLES)
    {
        rputs ("Количество переменных превысило допустимое значение!");
        error = true;
        return;
    }

    temp->IndexVar = count;                 // записать адрес переменной в массив ListVar
    strcpy(ListVar[count].NameVar, add);    // записать в список переменных имя переменной
    countVariables = count;
    temp->ThisIs = VARIABLE;                // определить тип данных лексемы
    ListVar[count].ValueVar = 0;            // по умолчанию значение новой переменной - ноль
    ListVar[count].Im_ValueVar = 0;
    ++countVariables;                       // одной переменной стало больше
    rprin ("\rСоздана новая переменная ");  // вывести на экран имя новой переменной
#ifdef WINDOWS
    puts (convCyrStr(add));
#else
    char *ptemp = get_utf8 (add);
    puts (ptemp);
    delete ptemp;
#endif
}
//===============================================================
// Метод вызывается, когда необходимо создать новую переменую с именем add, занеся её
// в массив пользовательских переменных. Метод используется при вычислении функции
// интегрирования.
void Interpreter::CreateVariable (char *add)
{
    int count = 0;
    for(; count < countVariables; ++count) // просмотреть массив переменных
    {
        // если в массиве ListVar есть переменная с именем add
        if (!strcmp (ListVar[count].NameVar, add))
        {
            return; // создавать переменную с именем add не нужно, она уже есть
        }
    }
    // иначе необходимо занести в массив новую переменную:

    if (count == QUANTITY_VARIABLES)
    {
        rputs ("Количество переменных превысило допустимое значение!");
        error = true;
        return;
    }

    strcpy(ListVar[count].NameVar, add);    // записать в массив переменных имя переменной
    countVariables = count;
    ListVar[count].ValueVar = 0;            // по умолчанию значение новой переменной - ноль
    ListVar[count].Im_ValueVar = 0;
    ++countVariables;                       // одной переменной стало больше
}
//===============================================================
// Меняет значение переменной add на указаные в параметрах. Метод используется при
// вычислении функции интегрирования.
void Interpreter::ChangeValueVariable (char *add, double Re_value, double Im_value)
{
    int count = 0;
    for(; count < countVariables; ++count) // просмотреть список переменных
    {
        // если в списке есть переменная с именем add, то изменить её значения
        if (!strcmp (ListVar[count].NameVar, add))
        {
            ListVar[count].ValueVar = Re_value;
            ListVar[count].Im_ValueVar = Im_value;
            return;
        }
    }
}
//===============================================================
// Метод возвращает истину, если в массиве пользовательских переменных ListVar есть
// переменная с именем name. Метод используется при объявлении новых пользовательских
// функций, имена функций и переменных не должны совпадать.
bool Interpreter::ExistsVariable (char *name)
{
    int count = 0;
    for(; count < countVariables; ++count) // просмотреть массив переменных
    {
        // если в массиве есть переменная с именем name
        if (!strcmp (ListVar[count].NameVar, name))
        {
            return true;
        }
    }
    return false;
}
//===============================================================
// Метод обновляет в списке лексем значения переменных и элементов массива.
// Применяется после выполнения блока в фигурных скобках.
void Interpreter::RefreshAllUserData ()
{
    class token *Next = start;
    while (Next)
    {
        if (Next->ThisIs == VARIABLE)
        {
            Next->number = ListVar[Next->IndexVar].ValueVar;
            Next->Im_number = ListVar[Next->IndexVar].Im_ValueVar;
        }
        else
            if (Next->ThisIs == NUMBER && Next->IndexArray > 0)
            {
                        // если массив указан как одномерный
        if (ListArray [Next->IndexArray].dim == 1)
        {
            // если индекс отрицательный или больше или равно размеру массива
            if (Next->j < 0 || Next->j >= ListArray [Next->IndexArray].j)
            {
                rprin ("Выход за пределы массива ");
                rprin (ListArray [Next->IndexArray].nameArray);
                error = true;
                return;
            }
            Next->number = ListArray [Next->IndexArray].elements[Next->j].Re;
            Next->Im_number = ListArray [Next->IndexArray].elements[Next->j].Im;
        }
        else
            if (ListArray [Next->IndexArray].dim == 2)
            {
                // если выход за пределы массива
                if (Next->j < 0 ||
                    Next->j >= ListArray [Next->IndexArray].i ||
                    Next->j < 0 ||
                    Next->j >= ListArray [Next->IndexArray].j)
                {
                    rprin ("Выход за пределы массива ");
                    rprin (ListArray [Next->IndexArray].nameArray);
                    error = true;
                    return;
                }
                Next->number = ListArray [Next->IndexArray].elements[Next->j].Re;
                Next->Im_number = ListArray [Next->IndexArray].elements[Next->j].Im;
            }
            }
        Next = Next->next;
    }
}

//===============================================================================================================
// МЕТОДЫ РАБОТЫ С ПОЛЬЗОВАТЕЛЬСКИМИ ФУНКЦИЯМИ:


// Метод удаляет из ListArray элементы в диапазоне от IndexBegin до IndexEnd,
// используется при разграничении области видимости объектов
void Interpreter::DelFunc (int IndexBegin, int IndexEnd)
{
    for (; IndexBegin < IndexEnd; ++IndexBegin)
    {
        ListFunctions[IndexBegin].name[0] = 0; // обнулить имя функции
        delete []ListFunctions[IndexBegin].body; // удалить из памяти тело функции
        ListFunctions[IndexBegin].body = 0; // обнулить тело функции
    }
}
//===============================================================
// Метод возвращает истину, если объявлена пользовательская функция с именем name.
// Метод применяется при создании новых пользовательских переменных, имена функций и
// переменных не должны совпадать.
bool Interpreter::ExistsFunction (char *name)
{
    int count = 0;
    for(; count < countFunctions; ++count) // просмотреть список переменных
    {
        // если в массиве ListFunctions есть функция с именем name
        if (!strcmp (ListFunctions[count].name, name))
        {
            return true;
        }
    }
    return false;
}
//===============================================================
// Функция считывает из строки string допустимое имя пользовательской функции и записывает
// его в name. Если подходящее имя не найдено, строку name оставляет нулевой.
// Правила выбора имени для функции те же, что и для переменной.
// Метод использует функция creatFunction.
void Interpreter::GetNameFunction (char *string, char *name)
{
    *name = 0; // обнулить строку
    if (isdigit (*string)) // имена функций не должны начинаться с цифр
        return;

    // цикл считывания имени функции
    while (*string &&
            (isalpha_(*string) ||
            isdigit (*string)  ||
            *string == '_') &&
            (name - &name[0] < MAX_NAME_USER_FUNCTION))
    {
        *name++ = *string++;
    }
    *name = 0;
}
//===============================================================
// Метод создает новую пользовательскую функцию, выполняет команду func в виде функции с
// двумя строковыми параметрами:
// func ("имя_функции", {тело_функции});
// Строки могут ограничиваться кавычками и фигурными скобками.
// Правила выбора имени для функции те же, что и для переменной.
// Пример: func ("f", {pi/2}) или func ("f", "pi/2"). Имя функции в кавычках можно не
// писать: func (f, {pi/2}).
// В теле функции может быть одно или несколько выражений, разделенных ЗАПЯТЫМИ, при этом
// функция всегда должна возвращать числовое значение (если логическое, то переводится в
// числовое). Если в теле одно выражение, то возвращается его результат, если несколько - то
// последнего выражения. Также можно использовать условия, циклы (if, for,... - это на самом
// деле обычные функции, возвращающие числовой ответ). При этом в функции в любом месте
// можно использовать функцию return, например: func (f, {return (pi/2)}), действует она
// аналогично оператору return в языке С++.
// Внутри функций можно объявлять переменные, область их видимости - тело данной функции.
// Функции и переменные текущего объекта "видны" в функции, при этом функция может изменить
// значения "глобальных" переменных. Объявлять функции внутри функций нельзя.
// Функции могут быть рекурсивными, т.е. вызывать сами себя. Однако создавать такие функции
// нельзя, так как стек программы будет быстро исчерпан и приложение закроется, допускается
// около 1000 рекурсивных вызовов.
// В целом многие правила создания функций взяты из языка С++.
// Пока функция не будет вызвана, тело её никак не проверяется на наличие ошибок и как и
// любая строка может содержать всё что угодно.
void Interpreter::creatFunction ()
{
    if (temp->ThisIs == FUNCTION) // если текущая лексема - команда func
    {
        if (temp->next &&
        temp->next->next) // если справа есть 2 лексемы
        {
            // если слева от func есть лексема или справа больше 2 лексем
            if (temp->prior || temp->next->next->next)
            {
                error = true;
                rprin ("Ошибка: функции объявляются в отдельном выражении");
                return;
            }

            // если первая справа лексема - имя функции, затем тело функции
            if (temp->next->ThisIs == STRING && temp->next->next->ThisIs == STRING)
            {
                // считать имя функции из второй лексемы и записать в последнюю строку
                // массива ListFunctions
                GetNameFunction (strings [temp->next->IndexString], ListFunctions[countFunctions].name);

                // если не удалось считать имя функции
                if (!ListFunctions[countFunctions].name[0])
                {
                    rprin ("\rНедопустимое имя функции!");
                    error = true;
                    return;
                }

                // если считанное имя - ключевое, зарезервированное
                if (EqualNamesUnique (ListFunctions[countFunctions].name))
                {
                    error = true;
                    rprin (ListFunctions[countFunctions].name);
                    rprin (" - ключевое слово, неприменимое для имени функции!");
                    return;
                }

                // если есть пользовательская функция с таким именем
                if (ExistsVariable (ListFunctions[countFunctions].name))
                {
                    error = true;
                    rprin (ListFunctions[countFunctions].name);
                    rprin (" - имя переменной, неприменимое для имени функции!");
                    return;
                }

                // если есть пользовательский массив с таким именем
                if (ExistsArray(ListFunctions[countFunctions].name))
                {
                    error = true;
                    rprin (ListFunctions[countFunctions].name);
                    rprin (" - имя массива, неприменимое для имени переменной!");
                    return;
                }

                // если уже объявлена функция с таким именем - переопределить функцию,
                // т.е. изменить её тело.
                int count = 0;
                for(; count < countFunctions; ++count) // просмотреть массив функций
                {
                    // если нашлась функция уже определена
                    if (!strcmp (ListFunctions[count].name, ListFunctions[countFunctions].name))
                    {
                        // скопировать в массив ListFunctions новое тело функции
                        strcpy (ListFunctions[count].body, strings [temp->next->next->IndexString]);
                        rprin ("Переопределилась функция ");
                        rprin (ListFunctions[countFunctions].name);
                        puts("");
                        DeleteAll (&start); // удалить из памяти весь список лексем
                        countToken = 0; // обнулить счетчик лексем
                        return;
                    }
                }

                if (countFunctions == MAX_QUANTITY_USER_FUNCTION)
                {
                    rputs ("Количество объявленных функций превысило допустимое значение!");
                    error = true;
                    return;
                }

                // определение новой пользовательской функции
                rprin ("Определена функция ");
                rputs (ListFunctions[countFunctions].name);

                // скопировать в массив ListFunctions тело функции
                ListFunctions[countFunctions].body = new char [MAX_STRING];
                if (!ListFunctions[countFunctions].body) // если не удалось выделить память
                    FatalError ("Ошибка выделения памяти."); // завершить приложение
                strcpy (ListFunctions[countFunctions].body, strings [temp->next->next->IndexString]);
                ++countFunctions; // одной функцией стало больше

                DeleteAll (&start); // удалить из памяти весь список лексем
                countToken = 0; // обнулить счетчик лексем
                return;
            } // if (temp->next->ThisIs == BRECKET_OPEN && ...
            else
            {
                error = true;
            }
        } // if (temp->next && ...
        else
        {
            error = true;
        }

    } // if (temp->ThisIs == FUNCTION)
}
//===============================================================
// Метод выполняет команду run, запуская пользовательскую функцию, имя которой хранится в
// виде строки в лексеме ptr_arg (имя хранится в массиве strings, а лексема содержит
// индекс строки в массиве). Команда run реализована в виде функции или функции-
// оператора с одним строковым параметром - именем функции.
// Метод возвращает комплексное число - результат функции. В слкчае ошибки в теле функции
// возвращает 0.
complex Interpreter::run_Function (class token *ptr_arg)
{
    if (ptr_arg->IndexString < 0) // в случае неправильного имени индекс может быть < 0
        ptr_arg->IndexString = 0;

    if (!strings [ptr_arg->IndexString][0]) // если пустая строка
    {
        rputs ("Недопустимое имя функции!");
        error = true;
        return null; // возвратить нулевое комплексное число
    }

    // если указано ключевое слово
    if (EqualNamesUnique (strings [ptr_arg->IndexString]))
    {
        error = true;
        rprin (strings [ptr_arg->IndexString]);
        rprin (" - ключевое слово, неприменимое для имени функции!");
        return null; // возвратить нулевое комплексное число
    }

    // если определена пользовательская переменная с таким именем
    if (ExistsVariable (strings [ptr_arg->IndexString]))
    {
        error = true;
        rprin (strings [ptr_arg->IndexString]);
        rprin (" - имя переменной, неприменимое для имени функции!");
        return null; // возвратить нулевое комплексное число
    }

    // если есть пользовательский массив с таким именем
    if (ExistsArray(strings [ptr_arg->IndexString]))
    {
        error = true;
        rprin (strings [ptr_arg->IndexString]);
        rprin (" - имя массива, неприменимое для имени переменной!");
        return null;
    }

    // если функция с указанным именем не была объявлена с помощью команды func
    if (!ExistsFunction (strings [ptr_arg->IndexString]))
    {
        error = true;
        rprin ("Ошибка! Необъявлена функция ");
        rprin (strings [ptr_arg->IndexString]);
        return null; // возвратить нулевое комплексное число
    }

    int count = 0;
    Interpreter *tmp; // объект для выполнения тела функции
    tmp = new Interpreter;
    if (!tmp) // если не удалось выделить память
        FatalError ("Ошибка выделения памяти."); // завершить приложение
    complex current; // ответ функции

     // просмотреть массив функций ListFunctions
    for(; count < countFunctions; ++count)
    {
        // если в массиве найдена нужная функция
        if (!strcmp (ListFunctions[count].name, strings [ptr_arg->IndexString]))
        {
            // передать в объект tmp пользовательские данные текущего объекта
            tmp->countVariables = this->countVariables;
            tmp->countFunctions = this->countFunctions;
            tmp->countArray = this->countArray;
            tmp->countString = this->countString;
            // флаг определяет, выполнилось ли утверждение return внутри функции
            return_func = false;
            // счетчик вызовов (в т.ч. рекурсивных) данной пользовательской функции
            ++count_recurs;
            // выполнить тело функции
            run_block (tmp, ListFunctions[count].body);
            // удалить из памяти локальные массивы в теле функции
            DelArrays (countArray, tmp->countArray);
            // удалить из памяти функции объявленные в теле функции
            DelFunc (countFunctions, tmp->countFunctions);
            // удалить из strings только что занесенные в него строки
            DeleteStrings (this->countString);
            current = answer;
            RefreshAllUserData (); // обновить пользовательские данные
            // возврат из функции - уменьшить на 1 счетчик рекурсии
            --count_recurs;
            // если возвратились из цепочки рекурсивных вызовов в первоначальный объект
            if (!count_recurs)
                return_func = false; // все функции return выполнены, сбросить флаг

            if (ThisComment) // если функция не возвратила никакого значения
            {
                rprin ("Функция ");
                rprin (ListFunctions[count].name);
                rprin (" не возвратила значения!");
                error = true;
                delete tmp;
                return null; // возвратить нулевое комплексное число
            }

            if (error) // если в теле функции найдены ошибки
            {
                delete tmp;
                rprin (" Ошибка в функции ");
                rprin (ListFunctions[count].name);
                return null; // возвратить нулевое комплексное число
            }
            delete tmp;
            return current; // вовзаратить результат функции
        }
    }
}

//===============================================================================================================
// ОТЛАДОЧНЫЕ МЕТОДЫ:

// метод просматривает список лексем и выводит тип каждой лексемы
void Interpreter::debug ()
{
    class token *Next = start;
    printf ("---------------\ncountToken = %i\n", countToken); // вывести текущее число лексем в списке
    while (Next)
    {
        switch (Next->ThisIs)
        {
        case NUMBER:
            printf ("is NUMBER = %f\n", Next->number);
            break;
            // оператор +
        case PLUS:
            puts("PLUS");
            break;
// знак или оператор -
        case MINUS:
            puts("MINUS");
            break;
// оператор умножения *
        case MULTIPLICATION:
            puts("MULTIPLICATION");
            break;
// оператор деления /
        case DIVISION:
            puts("DIVISION");
            break;
// открывающаяся скобка (
        case BRECKET_OPEN:
            puts("BRECKET_OPEN");
            break;
// закрывающаяся скобка )
        case BRECKET_CLOSE:
            puts("BRECKET_CLOSE");
            break;
// оператор запятая, разделяет параметры в функциях от нескольких аргументов,
        case COMMA:
            puts("COMMA");
            break;
// оператор присваивания = , a = 2
        case EQUAL:
            puts("EQUAL");
            break;
// тип данных пользовательская переменная
        case VARIABLE:
            printf ("is VARIABLE = %f\n", Next->number);
            break;
// логический тип данных
        case BOOL:
            puts("BOOL");
            break;
// строковый тип данных
        case STRING:
            puts("STRING");
            break;
// идентификатор - элемент массива
        case ELEMENT_ARRAY:
            puts("ELEMENT_ARRAY");
            break;
// открывающаяся квадратная скобка
        case SQUARE_BRECKET_OPEN:
            puts("SQUARE_BRECKET_OPEN");
            break;
// закрывающаяся квадратная скобка
        case SQUARE_BRECKET_CLOSE:
            puts("SQUARE_BRECKET_CLOSE");
            break;
        default:
            printf ("ThisIs = %i\n", Next->ThisIs);
        }
        Next = Next->next; // перейти к следующей лексеме
    }
    puts("----------------\n");
    //getchar();
}

//===============================================================================================================
// МЕТОДЫ ПОИСКА ОШИБОК:

// функция поиска некоторых синтаксических ошибок в выражении
void Interpreter::SearchErrors ()
{
    class token *temp;
    temp = start;
    while (temp) // просмотреть список, по очереди удалив все элементы
    {
        if (temp->next && temp->ThisIs == VARIABLE)
            if (temp->next->ThisIs == BRECKET_OPEN)
            {
                rprin ("Синтаксическая ошибка");
                error = true;
                return;
            }
        if (temp->ThisIs == NUMBER || temp->ThisIs == BOOL)
            if (temp->next) // справа от числа есть лексема
            {
                switch (temp->next->ThisIs) // что за лексема?
                {
                    case BRECKET_OPEN:
                        rprin ("Между числом '");
                        WriteComplexNumberInFile (temp->number, temp->Im_number, stdout);
                        rprin ("' и '(' нет оператора!");
                        error = true;
                        break;
                    case NUMBER:
                    case BOOL:
                        rprin ("Между числами '");
                        WriteComplexNumberInFile (temp->number, temp->Im_number, stdout);
                        rprin ("' и '");
                        WriteComplexNumberInFile (temp->next->number, temp->next->Im_number, stdout);
                        rprin ("' нет оператора!");
                        error = true;
                        break;
                    case EQUAL:
                        if ((temp->next->next))
                        {
                            if (temp->next->next->ThisIs != EQUAL)
                            {
                                rprin ("Оператор '=' инициализирует только переменные! '==' - оператор сравнения.");
                                error = true;
                            }
                            else error = false;
                        }
                        break;
                }

                if (IsFunction1Arg (temp->next) || IsFunction2Arg (temp->next))
                {
                    rprin ("Пропущен оператор между числом '");
                    WriteComplexNumberInFile (temp->number, temp->Im_number, stdout);
                    rprin ("' и функцией!");
                    error = true;
                }
            if (temp->prior)
            {
                if (temp->prior->ThisIs == BRECKET_CLOSE)
                {
                    rprin ("Между ')' и числом '");
                    WriteComplexNumberInFile (temp->number, temp->Im_number, stdout);
                    rprin ("' нет оператора!");
                    error = true;
                }
            }
            if (temp->prior)
            {
                if (temp->prior->ThisIs == BRECKET_CLOSE)
                {
                    rprin ("Между ')' и числом '");
                    WriteComplexNumberInFile (temp->number, temp->Im_number, stdout);
                    rprin ("' нет оператора!");
                    error = true;
                }
            }
        }
    temp = temp->next;
    }
}

//===============================================================================================================

// Функция просматривает строку, находит в ней лексемы и формирует двусвязный список лексем.
// Создаются пользовательские переменные и производится поиск некоторых синтаксических
// ошибок в списке.
void Interpreter::FillingListLexeme (char *str)
{
    int count_bytes = 0;                // счетчик символов, используется при считывании
    // из строки отдельных строк, а также любых слов, которые могли бы быть именами
    // переменных, функций или массивов.

    int readStringInQuotationMarks;     // флаг используется при считывании строк
    // заключенных в фигурные скобки

    bool begin = true;                  // флаг определяет, первой ли по счету в
                                        // список заносится лексема

    int countOpen, countClose,          // счетчики открывающихся и закрывающихся скобок,
    // используются при считывании строк заключенных в фигурные скобки

        countBrecketsOpen = 0,  // счетчики для круглых скобок
        countBrecketsClose = 0,
        countModule = 0;                // счетчик модулей |

    start = end = NULL;                 // перед заполнением списка лексем обнулить
                                        // указатели на начало и конец списка

    countToken = 0; // счетчики лексем обнуляtтся
//  countString = 0;

    OLDcountVariables = countVariables; // сохранить прежнее количество переменных
    OLDcountFunctions = countFunctions; // сохранить прежнее количество функций
    OLDcountArray     = countArray;     // сохранить прежнее количество массивов

// --- СОЗДАНИЕ СПИСКА ЛЕКСЕМ

    // Цикл заполнения списка лексем, т.е. анализ строки и
    // разделениt её на отдельные лексемы
    for (iteration = 0; ; ++iteration) // счетчик итераций цикла
    {
        if (iteration == LIMITE_ERROR)
            // если программа зациклилась из-за синтаксической ошибки, удалить список
            // из памяти и завершить работу функции. Данная ошибка чаще возможна при добавлении новых
            // функций, операторов, т.е. ключевых слов.
        {
             // вывести сообщение об ошибке
            rprin ("Синтаксическая ошибка: неизвестная команда!");
            error = true;       // глобальная переменная-флаг определяет наличие
                                // ошибок в выражении
            return;
        }

// --- НАЙТИ ЛЕКСЕМУ В СТРОКЕ
get_lexeme:;
        if (!*p) // если достигли конца строки, завершить цикл
            break;

        while (isspace (*p) && *p++); // пропускать пробелы между лексемами

        if (!*p) // если достигли конца строки, завершить цикл
            break;

        temp = new token; // после того как пропустили пробелы в строке, выделить
        // память для нового элемента списка

        if (!temp) // если не удалось выделить память
            FatalError ("Ошибка выделения памяти."); // завершить приложение

        temp->number = temp->Im_number = 0.; // новые переменные обнулить

        ++countToken; // предполагается, что найдена  лексема

// --- СЧИТАТЬ НАЙДЕННОЕ СЛОВО
        if (isalpha_ (*p)   || *p == '_') // если первый символ латинского
            // или русского алфавита или знак _
        {
            char *box, // в этот массив скопируется
                // имя предпологаемой переменной

                 *ptemp = p; // указатель для проверки методом EqualNamesUnique, является
                // ли её имя допустимым для переменной

            box = new char [SIZE_UNIQUE_WORD_AND_VARIABLES];
            if (!box) // если не удалось выделить память
                FatalError ("Ошибка выделения памяти."); // завершить приложение
            count_bytes = 0; // счетчик символов имени переменной
            ptemp = p;
            while ( // пока не достигнем конца выражения или не закончатся
                    // буквы или _ или пока не достигнем конца
                    // массива box, копируем в массив box имя переменной
                    (isalpha_ (*ptemp)  || *ptemp == '_' || isdigit(*ptemp))
                    && *ptemp
                  ) // имя переменной может состоять из символов лат. и рус.
                    // алфавитов, цифр, знака _
            {
                if (count_bytes == SIZE_UNIQUE_WORD_AND_VARIABLES-1)
                {
                    rputs("Ошибка: слишком длинное имя!");
                    delete temp;
                    error = true;
                    puts(box);
                    delete []box;
                    return;
                }
                box[count_bytes++] = *ptemp++; // копирование имени переменной
            }
            box[count_bytes] = 0; // обозначить конец массива

            //puts("");
            //puts(box);

// --- ЗАПИСЬ В СПИСОК ИМЕНИ ПОДИНТЕГРАЛЬНОЙ ПЕРЕМЕННОЙ

            // если в списке последние лексемы integral (строка,
            if (end && end->ThisIs == COMMA &&
                end->prior && end->prior->ThisIs == STRING &&
                end->prior->prior && end->prior->prior->ThisIs == BRECKET_OPEN &&
                end->prior->prior->prior && end->prior->prior->prior->ThisIs == INTEGRAL)
            {
                p = ptemp; // указатель переставить за имя функции
                temp->ThisIs = STRING; // определить тип лексемы в списке
                strings[countString] = new char [MAX_STRING]; // выделить память для строки
                if (!strings[countString])
                {
                    rprin ("Ошибка выделения памяти!");
                    error = true;
                    delete temp;
                    delete []box;
                    return;
                }
                temp->IndexString = countString++; // одной строкой стало больше.
                    // Все пользовательские строки хранятся в глобальном массиве strings.
                    // число temp->IndexString - индекс строки в массиве strings

                // если массив strings полностью заполнен строками
                if (countString > QUANTITY_STRINGS)
                {
                    rputs ("Ошибка: слишком много строк в одном выражении!");
                    delete temp;
                    delete []box;
                    error = true;
                    return;
                }
                strcpy (strings[temp->IndexString], box); // скопировать строку в strings
                delete []box;
                goto add; // занести лексему в список
            }

// --- ЗАПИСЬ В СПИСОК ИМЕНИ ПОЛЬЗОВАТЕЛЬСКОЙ ФУНКЦИИ ИЛИ МАССИВА
            // если в списке последние лексемы func или run или new(
            if (ExistsFunction (box) ||
                (end  && end->ThisIs == FUNCTION) ||
                 (end && end->ThisIs == RUN) ||
                 (end && end->prior && end->prior->ThisIs == NEW))
                 // если определена пользовательская функция с таким именем и  последняя
                 // лексема в списке - функция FUNCTION или RUN, записать в список строку
                 // с пользовательской функцией.
                 // Имена пользовательских функций можно не писать в кавычках, например
                 // run(my_fync);run my_fync;run("my_func");run "my_func" - одно и то же.
            {
                p = ptemp; // указатель переставить за имя функции
                temp->ThisIs = STRING; // определить тип лексемы в списке
                strings[countString] = new char [MAX_STRING];
                if (!strings[countString])
                {
                    rprin ("Ошибка выделения памяти!");
                    error = true;
                    delete temp;
                    delete []box;
                    return;
                }
                temp->IndexString = countString++; // одной строкой стало больше.
                    // Все пользовательские строки хранятся в массиве strings.
                    // число temp->IndexString - индекс строки в массиве strings

                // если массив strings полностью заполнен строками
                if (countString > QUANTITY_STRINGS)
                {
                    rputs ("Ошибка: слишком много строк в одном выражении!");
                    delete temp;
                    delete []box;
                    error = true;
                    return;
                }
                strcpy (strings[temp->IndexString], box); // скопировать строку в strings

                // если последние лексемы в списке new(
                if (end && end->prior && end->prior->ThisIs == NEW)
                {
                    // если box не ключевое слово и не имя функции или переменной
                    if (!EqualNamesUnique (box) &&
                        !ExistsFunction (box) &&
                        !ExistsVariable(box))
                    {
                        // если есть массив с таким именем
                        if (ExistsArray(box))
                        {
                            // записать в текущую лексему индекс существующего массива
                            temp->IndexArray = count; // глобальная переменная
                            delete []box;
                            goto add; // занести лексему в список
                        }

                        // занести в массив пользовательских массивов имя нового массива
                        strcpy (ListArray[countArray].nameArray, box);
                        // записать в текущую лексему индекс массива
                        temp->IndexArray = countArray++;
                        // обнулить указатель на массив комплексных чисел
                        ListArray[countArray].elements = 0;
                        // нулевой dim определяет, что массив не переопределяется
                        ListArray[countArray].dim = 0;
                        if (countArray == MAX_QUANTITY_USER_ARRAY)
                        {
                            rputs ("Ошибка: слишком много массивов!");
                            delete temp;
                            delete []box;
                            error = true;
                            return;
                        }
                    }
                } // if (end && end->prior && end->prior->ThisIs == NEW)
                delete []box;
                goto add; // занести лексему в список
            }

// --- ЗАПИСЬ В СПИСОК ЭЛЕМЕНТА МАССИВА - например a[index]
            // если есть массив с таким именем
            if (ExistsArray (box))
            {
                p = ptemp; // указатель переставить за имя массива
                temp->ThisIs = ELEMENT_ARRAY; // определить тип лексемы в списке
                // записать в текущую лексему индекс существующего массива
                temp->IndexArray = count;
                delete []box;
                p = ptemp; // указатель переставить за имя массива
                goto add; // занести лексему в список
            }

// --- ЗАПИСЬ В СПИСОК ПЕРЕМЕННОЙ
            // если не ключевое слово
            if (!EqualNamesUnique (box) && (
                !(*p != 'i' && !*(p+1)) || // если это не комплексное число i
                !(*p == 'i' && isdigit(*(p+1)))))

                // если найденное слово не совпадает ни с одним именем
                // функций/операторов/констант без учета регистра, то записать
                // в массив ListVar новую переменную
            {
                AddVariableInList (box);
                // метод проверяет, имеется ли уже в списке данная переменная
                // (с учетом регистра), если нет, переменная заносится в список,
                // ей присваивается нулевое значение, иначе текущей лексеме
                // присваивается значение уже имеющейся переменной

                if (error) // если количество переменных превысило допустимое
                {
                    rputs("Ошибка: слишком много переменных!");
                    delete temp; // освободить только что выделенную память
                    error = true;
                    delete []box;
                    return;
                }

                temp->ThisIs = VARIABLE; // определить тип лексемы в списке
                p = ptemp; // пропустить имя переменной
                delete []box;
                goto add;  // добавление лексемы в список
            }
            delete []box;
        } // if (isalpha_ (*p)  || *p == '_')
//------------------------------------------------------------------------------

// выяснить, является найденная лексема функцией, оператором, константой или числом
        switch (*p)
        {
            case ';': // символ конца выражения

            delete temp; // освободить только что выделенную память
            --countToken; // одной лексемой стало меньше
            return; // перейти к циклу вычисления выражения
//------------------------------------------------------------------------------
            case '"': // строка, ограниченная кавычками

                ++p; // перейти к первому символу строки
                temp->ThisIs = STRING; // определить тип лексемы в списке
                strings[countString] = new char [MAX_STRING];
                if (!strings[countString])
                {
                    rprin ("Ошибка выделения памяти!");
                    error = true;
                    delete temp;
                    return;
                }
                temp->IndexString = countString++; // одной строкой стало больше.
                // Все пользовательские строки хранятся в массиве strings.
                // число temp->IndexString - индекс строки в массиве strings

                 // если массив strings полностью заполнен строками
                if (countString > QUANTITY_STRINGS)
                {
                    rputs ("Ошибка: слишком много строк в одном выражении!");
                    delete temp;
                    error = true;
                    return;
                }
                count_bytes = 0; // счетчик символов

                get_symbol: while (*p != '"') // запись в массив strings найденной строки
                {
                    if (!*p) // конец выражения?
                    {
                        rputs("Cинтаксическая ошибка: пропущен знак '\"'");
                        delete temp;
                        error = true;
                        return;
                    }

                    if (count_bytes == MAX_STRING-1)
                    {
                        rputs ("Ошибка: слишком длинная строка!");
                        delete temp;
                        error = true;
                        return;
                    }

                    // в строках могут встречаться сочетания символов \n и \t,
                    // которые переводятся в один нужный символ
                    if (*p == '\\') // найдена косая черта
                    {
                        switch (*(p+1))
                        {
                        case 't': // символ табуляции
                            ++p;
                            *p = '\t';
                            break;
                        case 'n': // символ новой строки
                            ++p;
                            *p = '\n';
                            break;
                        }
                    }
                     // запись строки в массив strings
                    strings [temp->IndexString][count_bytes++] = *p++;
                }

                 // сочетание символов \" означает, что строка еще не закончилась,
                 // а кавычки будут содержаться внутри строки
                if (*(p-1) == '\\')
                {
                    count_bytes -= 1; // косую черту не записывать в строку
                    strings [temp->IndexString][count_bytes++] = '"'; // убрать из
                    // строки косую черту перед кавычками

                    p++; // перейти к следующему символу
                    goto get_symbol; // считать следующий символ
                }
                strings [temp->IndexString][count_bytes] = 0; // обозначить конец строки
                p++; // перейти к следующему символу

                // запись имени масива
                // если последние лексемы в списке new(
                if (end && end->prior && end->prior->ThisIs == NEW)
                {
                    // если не ключевое слово и не имя функции или переменной
                    if (!EqualNamesUnique (strings [temp->IndexString]) &&
                        !ExistsFunction (strings [temp->IndexString]) &&
                        !ExistsVariable(strings [temp->IndexString]))
                    {
                        // если есть массив с таким именем
                        if (ExistsArray(strings [temp->IndexString]))
                        {
                            // записать в текущую лексему индекс существующего массива
                            temp->IndexArray = count;
                            goto add; // занести лексему в список
                        }

                        // занести в массив пользовательских массивов имя нового массива
                        strcpy (ListArray[countArray].nameArray, strings [temp->IndexString]);
                        // записать в текущую лексему индекс массива
                        temp->IndexArray = countArray++;
                        // обнулить указатель на массив комплексных чисел
                        ListArray[countArray].elements = 0;
                        // нулевой dim определяет, что массив не переопределяется
                        ListArray[countArray].dim = 0;
                        if (countArray == MAX_QUANTITY_USER_ARRAY)
                        {
                            rputs ("Ошибка: слишком много массивов!");
                            delete temp;
                            error = true;
                            return;
                        }
                    }
                } // if (end && end->prior && end->prior->ThisIs == NEW)
            break;
//------------------------------------------------------------------------------
            case '{': // строка, ограничена фигурными скобками

                ++p; // перейти к первому символу строки
                temp->ThisIs = STRING; // определить тип лексемы в списке
                strings[countString] = new char [MAX_STRING];
                if (!strings[countString])
                {
                    rprin ("Ошибка выделения памяти!");
                    error = true;
                    delete temp;
                    return;
                }
                temp->IndexString = countString++; // одной строкой стало больше.
                // Все пользовательские строки хранятся в массиве strings.
                // число temp->IndexString - индекс строки в массиве strings

                 // если массив strings полностью заполнен строками
                if (countString > QUANTITY_STRINGS)
                {
                    rputs ("Ошибка: слишком много строк в одном выражении!");
                    delete temp;
                    error = true;
                    return;
                }
                count_bytes = 0; // счетчик символов обнулить
                countOpen = 1; //
                countClose = 0;
                readStringInQuotationMarks = 1; // флаг указывает, что производится
                // считывание строки в кавычках - между кавычек фигурные скобки не
                // учитываются

                 // запись в массив strings найденной строки
                while (countOpen != countClose) // пока не достигнем последней }
                {
                    if (!*p) // конец выражения?
                    {
                        rputs("Cинтаксическая ошибка: пропущен знак '}'");
                        delete temp;
                        error = true;
                        return;
                    }

                    if (count_bytes == MAX_STRING-1)
                    {
                        rputs ("Ошибка: слишком длинная строка!");
                        delete temp;
                        error = true;
                        return;
                    }

                    // если найдены " и перед ними не стоит '\'
                    if (*p == '"' && *(p-1) != '\\')
                    {
                        readStringInQuotationMarks *= -1; // инверсировать значение флага
                    }
                    else
                    {
                        // в строках могут встречаться сочетания символов \n и \t
                        // которые переводятся в один нужный символ
                        if (*p == '\\') // найдена косая черта
                        {
                            switch (*(p+1))
                            {
                            case 't': // символ табуляции
                                ++p;
                                *p = '\t';
                                break;
                            case 'n': // символ новой строки
                                ++p;
                                *p = '\n';
                                break;
                            } // switch (*(p+1))
                        } // if (*p == '\\') // найдена косая черта
                        else
                            // внутри кавычек фигурные скобки не подсчитывать
                            if (readStringInQuotationMarks != -1)
                            {
                                if (*p == '{')
                                {
                                    if (*(p-1) != '\\') // сочетание \{ не учитывать
                                    {
                                        ++countOpen;
                                    }
                                    else
                                    {
                                        count_bytes -= 2;
                                    }
                                }
                                else
                                {
                                    if (*p == '}')
                                    {
                                        if (*(p-1) != '\\') // сочетание \} не учитывать
                                        {
                                            ++countClose;
                                        }
                                        else
                                        {
                                            count_bytes -= 2;
                                        }
                                    } // if (*p == '}')
                                } // else
                            } // if (readStringInQuotationMarks != -1)
                    }

                    // запись строки в массив strings
                    strings [temp->IndexString][count_bytes++] = *p++;
                }

                strings [temp->IndexString][count_bytes-1] = 0; // обозначить конец строки
                count_bytes = 0;
                // запись имени масива
                // если последние лексемы в списке new(
                if (end && end->prior && end->prior->ThisIs == NEW)
                {
                    // если не ключевое слово и не имя функции или переменной
                    if (!EqualNamesUnique (strings [temp->IndexString]) &&
                        !ExistsFunction (strings [temp->IndexString]) &&
                        !ExistsVariable(strings [temp->IndexString]))
                    {
                        // если есть массив с таким именем
                        if (ExistsArray(strings [temp->IndexString]))
                        {
                            temp->IndexArray = count;
                            goto add; // занести лексему в список
                        }
                        // занести в массив пользовательских массивов имя нового массива
                        strcpy (ListArray[countArray].nameArray, strings [temp->IndexString]);
                        // записать в текущую лексему индекс массива
                        temp->IndexArray = countArray++;
                        // обнулить указатель на массив комплексных чисел
                        ListArray[countArray].elements = 0;
                        // нулевой dim определяет, что массив не переопределяется
                        ListArray[countArray].dim = 0;

                        if (countArray == MAX_QUANTITY_USER_ARRAY)
                        {
                            rputs ("Ошибка: слишком много массивов!");
                            delete temp;
                            error = true;
                            return;
                        }
                    }
                } // if (end && end->prior && end->prior->ThisIs == NEW)
                goto add;
            break;
//------------------------------------------------------------------------------
            case '=': // оператор присваивания = или сравнения ==

                ++p;
                temp->ThisIs = EQUAL; // оператор присваивания =
                if (end) // если в списке уже есть лексемы
                {
                    switch (end->ThisIs) // какая в списке последняя лексема?
                    {
                    case EQUAL: // если оператор присваивания =, то перевести его
                        // в один оператор сравнения ==

                        end->ThisIs = EGALE; // переопределить последнюю лексему в
                        // оператор сравнения

                        --countToken;
                        delete temp; // не заносить текущую лексему в список
                        goto get_lexeme; // найти в строке str новую лексему
                    break;

                    case GREATER_THAN: // оператор >= аналогично
                        end->ThisIs = GREATER_OR_EQUAL;
                        --countToken;
                        delete temp;
                        goto get_lexeme;
                    break;

                    case LESS_THAN: // оператор <= аналогично
                        end->ThisIs = LESS_OR_EQUAL;
                        --countToken;
                        delete temp;
                        goto get_lexeme;
                    break;
                    }
                }
            break;
//------------------------------------------------------------------------------
            case ',': // разделитель аргументов в функциях или отдельных частей выражения

                ++p;  // перейти к следующему символу строки
                temp->ThisIs = COMMA;
            break;
//------------------------------------------------------------------------------
            case '.': // оператор точка, служит разделителем имени переменной и
                // комплексной или действительной части значения переменной (как
                // доступ к членам структур в языке С++)

                ++p;
                temp->ThisIs = POINT;
            break;
//------------------------------------------------------------------------------
            case '+': // знак числа или оператор сложения

                ++p;
                temp->ThisIs = PLUS;
            break;
//------------------------------------------------------------------------------
            case '-': // знак числа или оператор вычитания

                ++p;
                temp->ThisIs = MINUS;
            break;
//------------------------------------------------------------------------------
            case '*': // оператор умножения

                ++p;
                temp->ThisIs = MULTIPLICATION;
            break;
//------------------------------------------------------------------------------
            case '#': // разделитель целой части, числителя и знаменателя в дроби

                ++p;
                temp->ThisIs = FRACTION;
            break;
//------------------------------------------------------------------------------
            case '/': // это знак деления или комментарии?

                ++p;  // перейти к следующей лексеме
                if (*p == '*') // если комментарии типа /* ... */
                {
                    --countToken; // комментарии в список не заносятся, зря выделили
                    // память и увеличили счетчик лексем

                    delete temp;  // освободить только что выделенную память

                     // если в строке не найдено сочетание */
                    if (!(p = strstr (p+1, "*/")))
                    {
                        rprin("!!!Не хватает закрывающего комментария \"*/\" !");
                        error = true;
                        return;
                    }
                    else // если найден закрывающий комментарий
                    {
                        p += 2;          // пропустить в строке пару символов */,
                        // указатель перед ними поставило утверждение p = strstr (p+1, "*/")

                        goto get_lexeme; // перейти вверх к циклу пропуска пробелов
                        // в строке, найти новую лексему
                    }
                }
                if (*p == '/') // если комментарии типа //
                {
                    delete temp; // освободить только что выделенную память
                    --countToken;
                    return;  // комментарии и всё что после них игнорируется,
                    // перейти к циклу вычисления выражения
                }
                temp->ThisIs = DIVISION; // остался один вариант: здесь имеется
                // оператор деления
            break;
//------------------------------------------------------------------------------
            case '^': // оператор возведения в степень

                ++p;
                temp->ThisIs = DEGREE;
            break;
//------------------------------------------------------------------------------
            case '(': // открывающаяся скобка

            // считывание условия, заключенного в ()
            // если последняя лексема в списке - оператор if, while или for
            if (end && (end->ThisIs == IF ||
                end->ThisIs == WHILE ||
                end->ThisIs == FOR))
            {
                ++p; // перейти к первому символу строки
                temp->ThisIs = STRING; // определить тип лексемы в списке
                strings[countString] = new char [MAX_STRING];
                if (!strings[countString])
                {
                    rprin ("Ошибка выделения памяти!");
                    error = true;
                    delete temp;
                    return;
                }
                temp->IndexString = countString++; // одной строкой стало больше.
                // Все пользовательские строки хранятся в массиве strings.
                // число temp->IndexString - индекс строки в массиве strings

                 // если массив strings полностью заполнен строками
                if (countString > QUANTITY_STRINGS)
                {
                    rputs ("Ошибка: слишком много строк в одном выражении!");
                    //puts("");
                    //printf("%d\n", countString);
                    delete temp;
                    error = true;
                    return;
                }
                count_bytes = 0; // счетчик символов обнулить
                countOpen = 1; //
                countClose = 0;
                readStringInQuotationMarks = 1; // флаг указывает, что производится
                // считывание строки в кавычках - между кавычек фигурные скобки не
                // учитываются

                 // запись в массив strings найденной строки
                while (countOpen != countClose) // пока не достигнем последней }
                {
                    if (!*p) // конец выражения?
                    {
                        rputs("Cинтаксическая ошибка: пропущен знак ')'");
                        delete temp;
                        error = true;
                        return;
                    }

                    if (count_bytes == MAX_STRING-1)
                    {
                        rputs ("Ошибка: слишком длинная строка!");
                        delete temp;
                        error = true;
                        return;
                    }

                    // если найдены " и перед ними не стоит '\'
                    if (*p == '"' && *(p-1) != '\\')
                    {
                        readStringInQuotationMarks *= -1; // инверсировать значение флага
                    }
                    else
                    {
                        // в строках могут встречаться сочетания символов \n и \t
                        // которые переводятся в один нужный символ
                        if (*p == '\\') // найдена косая черта
                        {
                            switch (*(p+1))
                            {
                            case 't': // символ табуляции
                                ++p;
                                *p = '\t';
                                break;
                            case 'n': // символ новой строки
                                ++p;
                                *p = '\n';
                                break;
                            } // switch (*(p+1))
                        } // if (*p == '\\') // найдена косая черта
                        else
                            // внутри кавычек  скобки не подсчитывать
                            if (readStringInQuotationMarks != -1)
                            {
                                if (*p == '(')
                                {
                                    ++countOpen;
                                }
                                else
                                {
                                    if (*p == ')')
                                    {
                                        ++countClose;
                                    } // if (*p == ')')
                                } // else
                            } // if (readStringInQuotationMarks != -1)
                    }

                    // запись строки в массив strings
                    strings [temp->IndexString][count_bytes++] = *p++;
                }

                strings [temp->IndexString][count_bytes-1] = 0; // обозначить конец строки
                count_bytes = 0;
                goto add;
            }
                ++countBrecketsOpen;
                ++p;
                temp->ThisIs = BRECKET_OPEN;
            break;
//------------------------------------------------------------------------------
            // скобки для обозначения индекса элемента массива
            case '[':
                ++p;
                temp->ThisIs = SQUARE_BRECKET_OPEN;
            break;
            case ']':
                ++p;
                temp->ThisIs = SQUARE_BRECKET_CLOSE;
            break;
//------------------------------------------------------------------------------
            case ')': // закрывающаяся скобка

                ++countBrecketsClose;
                ++p;
                temp->ThisIs = BRECKET_CLOSE;
            break;
//------------------------------------------------------------------------------
            case '|': // оператор модуля числа

                ++countModule;
                ++p;
                temp->ThisIs = MODULE;
            break;
//------------------------------------------------------------------------------
            case '!': // оператор факториала числа

                ++p;
                temp->ThisIs = FACTORIAL;
            break;
//------------------------------------------------------------------------------
            case '>': // логический оператор больше

                ++p;
                temp->ThisIs = GREATER_THAN;

                if (end)  // если в списке уже есть лексемы
                    if (end->ThisIs == LESS_THAN) // оператор на равно <> аналогично ==
                    {
                        delete temp;
                        --countToken;
                        end->ThisIs = NE_EGALE;
                        goto get_lexeme;
                    }
            break;
//------------------------------------------------------------------------------
            case '<': // логический оператор меньше

                ++p;
                // если последняя лексема в списке - оператор <
                if (end)
                    if (end->ThisIs == LESS_THAN) // замена двух < на оператор <<
                    {
                        delete temp;
                        --countToken;
                        end->ThisIs = COUT2;
                        goto get_lexeme;
                    }
                temp->ThisIs = LESS_THAN;
            break;
//------------------------------------------------------------------------------
            case 'a':

                 // регистр символов не учитывать, метка ans - результат предыдущего
                 // вычисления
                if ( (*(1+p)) == 'n' &&
                     (*(2+p)) == 's')
                {
                    temp->ThisIs = NUMBER;
                    temp->number = answer.Re;  // предыдущее значение вычисления берется
                    // из глобальной переменной answer

                    temp->Im_number = answer.Im;
                    p += 3; // в слове ans три буквы, поэтому пропускаем их
                }
                else
                    if ( (*(1+p)) == 'b' &&
                         (*(2+p)) == 's') // функция abs
                    {
                        temp->ThisIs = ABSVALUE;
                        p += 3;
                    }
                    else
                        if ( (*(1+p)) == 'c' &&
                             (*(2+p)) == 'o' &&
                             (*(3+p)) == 's') // функция acos
                        {
                            temp->ThisIs = ACOS;
                            p += 4;
                        }
                        else
                            if ( (*(1+p)) == 's' &&
                                 (*(2+p)) == 'i' &&
                                 (*(3+p)) == 'n') // функция asin
                            {
                                temp->ThisIs = ASIN;
                                p += 4;
                            }
                            else
                                if ( (*(1+p)) == 't' &&
                                     (*(2+p)) == 'g') // функция atg
                                {
                                    temp->ThisIs = ATAN;
                                    p += 3;
                                }
                                else
                                    if ( (*(1+p)) == 'c' &&
                                         (*(2+p)) == 't' &&
                                         (*(3+p)) == 'g') // функция actg
                                    {
                                        temp->ThisIs = ACTAN;
                                        p += 4;
                                    }
                                    else
                                        if ( (*(1+p)) == 'r' &&
                                             (*(2+p)) == 'g')
                                            // аргумент комплексного числа
                                        {
                                            temp->ThisIs = ARG;
                                            p += 3;
                                        }
                                        else
                                            if ( (*(1+p)) == 's' &&
                                                 (*(2+p)) == 'h')
                                                // гиперболический арксинус
                                            {
                                                temp->ThisIs = ASH;
                                                p += 3;
                                            }
                                            else
                                                if ( (*(1+p)) == 'c' &&
                                                     (*(2+p)) == 'h')
                                                    // гиперболический арккосинус
                                                {
                                                    temp->ThisIs = ACH;
                                                    p += 3;
                                                }
                                                else
                                                    if ( (*(1+p)) == 't' &&
                                                         (*(2+p)) == 'h')
                                                        // гиперболический арктангенс
                                                    {
                                                        temp->ThisIs = ATH;
                                                        p += 3;
                                                    }
                                                    else
                                                        if ( (*(1+p)) == 'c' &&
                                                             (*(2+p)) == 't' &&
                                                             (*(3+p)) == 'h')
                                                            // гиперболический арккотангенс
                                                        {
                                                            temp->ThisIs = ACTH;
                                                            p += 4;
                                                        }
                                                        else
                                                            if ( (*(1+p)) == 'n' &&
                                                                 (*(2+p)) == 'd')
                                                                // логический оператор И
                                                            {
                                                                temp->ThisIs = AND;
                                                                p += 3;
                                                            }

            break;
//------------------------------------------------------------------------------
            case 'b':
                if (*(1+p) == 'r' &&
                    *(2+p) == 'e' &&
                    *(3+p) == 'a' &&
                    *(4+p) == 'k')
                    // оператор break
                {
                    temp->ThisIs = BREAK;
                    p += 5;
                }
            break;
//------------------------------------------------------------------------------
            case 'c':

                if ( (*(1+p)) == 'o' &&
                     (*(2+p)) == 's') // функция cos
                {
                    temp->ThisIs = COS;
                    p += 3;
                }
                else
                    if ( (*(1+p)) == 't' &&
                         (*(2+p)) == 'g') // функция ctg
                    {
                        temp->ThisIs = CTG;
                        p += 3;
                    }
                        else
                            if ( (*(1+p)) == 'u' &&
                                 (*(2+p)) == 'b') // функция cub
                            {
                                temp->ThisIs = CUB;
                                p += 3;
                            }
                            else
                                if ( (*(1+p)) == 'b' &&
                                     (*(2+p)) == 'r' &&
                                     (*(3+p)) == 't') // функция cbrt
                                {
                                    temp->ThisIs = CBRT;
                                    p += 4;
                                }
                                else
                                    if ( (*(1+p)) == 'o' &&
                                         (*(2+p)) == 'n' &&
                                         (*(3+p)) == 'j')
                                        // функция комплесно-сопряженного conj
                                    {
                                        temp->ThisIs = CONJ;
                                        p += 4;
                                    }
                                    else
                                        if ( (*(1+p)) == 'h')
                                            // гиперболический косинус ch
                                        {
                                            temp->ThisIs = CH;
                                            p += 2;
                                        }
                                        else
                                            if ( (*(1+p)) == 't' &&
                                                 (*(2+p)) == 'h')
                                                // гиперболический арктангенс ath
                                            {
                                                temp->ThisIs = CTH;
                                                p += 3;
                                            }
                                            else
                                                if ( (*(1+p)) == 's' &&
                                                     (*(2+p)) == 'e' &&
                                                     (*(3+p)) == 'c')
                                                    // функция косеканс csec
                                                {
                                                    temp->ThisIs = CSEC;
                                                    p += 4;
                                                }
                                                else
                                                    if (*(1+p) == 'o' &&
                                                        *(2+p) == 'n' &&
                                                        *(3+p) == 't' &&
                                                        *(4+p) == 'i' &&
                                                        *(5+p) == 'n' &&
                                                        *(6+p) == 'u' &&
                                                        *(7+p) == 'e')
                                                        // оператор continue
                                                    {
                                                        temp->ThisIs = CONTINUE;
                                                        p += 8;
                                                    }
                                                    else
                                                        if (*(1+p) == 'o' &&
                                                        *(2+p) == 'u' &&
                                                        *(3+p) == 't')
                                                        // идентификатор cout
                                                        {
                                                            temp->ThisIs = COUT1;
                                                            p += 4;
                                                        }
                                                        else
                                                        //if (*p == 'c')
                                                        // константа с - скорость света, м/с
                                                        {
                                                            temp->ThisIs = NUMBER;
                                                            temp->number = k_c;
                                                            p++;
                                                        }
            break;
//------------------------------------------------------------------------------
            case 'd':

                if ( (*(1+p)) == 'i' &&
                     (*(2+p)) == 'v') // оператор div
                {
                    temp->ThisIs = DIV;
                    p += 3;
                }
                else
                    if ( (*(1+p)) == 'o') // оператор do
                    {
                        temp->ThisIs = DO;
                        p += 2;
                    }
            break;
//------------------------------------------------------------------------------
            case 'e':
            case 'E':

                if ( *p == 'e' &&
                    (*(1+p)) == 'x' &&
                    (*(2+p)) == 'p') // функция exp
                {
                    temp->ThisIs = EXP;
                    p += 3;
                }
                else
                    if (*p == 'E' &&
                        (*(1+p)) == 'o') // константа "Эпсилон нулевое"
                    {
                        temp->ThisIs = NUMBER;
                        temp->number = k_e0;
                        p += 2;
                    }
                    else
                        if (*(1+p) == 'l' &&
                            *(2+p) == 's' &&
                            *(3+p) == 'e')
                            // оператор else
                        {
                            p += 4;
                            temp->ThisIs = ELSE;
                        }
                        else
                            if (*(1+p) == 'x' &&
                                *(2+p) == 'i' &&
                                *(3+p) == 't')
                                // команда exit
                                {
                                    p += 4;
                                    temp->ThisIs = EXIT;
                                }
                            else
                                if (*(1+p) == 'x' &&
                                    *(2+p) == 'a' &&
                                    *(3+p) == 'c' &&
                                    *(4+p) == 't')
                                    // функция exact
                                    {
                                        p += 5;
                                        temp->ThisIs = EXACT;
                                    }
                                    else
                                    //if (*p == 'e') // константа е - заряд электрона
                                    {
                                        temp->ThisIs = NUMBER;
                                        temp->number = k_e;
                                        ++p;
                                    }
            break;
//------------------------------------------------------------------------------
            case 'f':

                if ( (*(1+p)) == 'a' &&
                     (*(2+p)) == 'l' &&
                     (*(3+p)) == 's' &&
                     (*(4+p)) == 'e')
                     // логическая константа false
                {
                    temp->ThisIs = BOOL;
                    p += 5;
                }
                else
                    if ( (*(1+p)) == 'o' &&
                         (*(2+p)) == 'r')
                         // оператор цикла for
                    {
                        temp->ThisIs = FOR;
                        p += 3;
                    }
                    else
                        if ( (*(1+p)) == 'u' &&
                             (*(2+p)) == 'n' &&
                             (*(3+p)) == 'c') // функция func объявления
                            // пользовательской функции
                        {
                            if (countToken > 1)
                            {
                                rputs ("Ошибка: функции объявляются в отдельном выражении");
                                error = true;
                                delete temp;
                                return;
                            }
                            temp->ThisIs = FUNCTION;
                            p += 4;
                        }
            break;
//------------------------------------------------------------------------------
            case 'G':
            case 'g':

                if ( *p == 'g' &&
                    (*(1+p)) == 'e' &&
                    (*(2+p)) == 't' &&
                    (*(3+p)) == 'r' &&
                    (*(4+p)) == 'e') // функция getre
                {
                    temp->ThisIs = GETRE;
                    p += 5;
                }
                else
                    if ( *p == 'g' &&
                        (*(1+p)) == 'e' &&
                         (*(2+p)) == 't' &&
                         (*(3+p)) == 'i' &&
                         (*(4+p)) == 'm') // функция getim
                    {
                        temp->ThisIs = GETIM;
                        p += 5;
                    }
                    else
                    if (*p == 'G')
                    {
                        temp->number = k_G; // гравитационная постоянная
                        temp->ThisIs = NUMBER;
                        ++p;
                    }
                    else
                        //if (*p == 'g')// ускорение свободного падения
                        {
                            temp->number = k_g;
                            temp->ThisIs = NUMBER;
                            ++p;
                        }
            break;

//------------------------------------------------------------------------------
            case 'h':

                if ( (*(1+p)) == 'y' &&
                     (*(2+p)) == 'p' &&
                     (*(3+p)) == 'o' &&
                     (*(4+p)) == 't') // функция hypot
                {
                    temp->ThisIs = HYPOT;
                    p += 5;
                }
                else
                    //if (*p == 'h') // постоянная Планка
                    {
                        temp->ThisIs = NUMBER;
                        temp->number = k_h;
                        ++p;
                    }
            break;
//------------------------------------------------------------------------------
            case 'i':

                if ( (*(1+p)) == 'n' &&
                     (*(2+p)) == 't' &&
                     (*(3+p)) == 'e' &&
                     (*(4+p)) == 'g' &&
                     (*(5+p)) == 'r' &&
                     (*(6+p)) == 'a' &&
                     (*(7+p)) == 'l')
                    // функция вычисления определенного интеграла Integral
                {
                    temp->ThisIs = INTEGRAL;
                    p += 8;
                }
                else
                    if ( (*(1+p)) == 'm')
                        // идентификатор комплексной части значения переменной im
                    {
                        p += 2;
                        temp->ThisIs = MEMBER_STRUCT_IM;
                    }
                    else
                        if ( (*(1+p)) == 'f') // оператор if
                        {
                            p += 2;
                            temp->ThisIs = IF;
                        }
                        else
                            //if (*p == 'i') // число i
                            {
                                ++p; // перейти к следующему символу
                                temp->Im_number = 1.;
                                temp->ThisIs = NUMBER;
                            }

            break;
//------------------------------------------------------------------------------
            case 'k': // постоянная Больцмана

                temp->number = k_k;
                temp->ThisIs = NUMBER;
                ++p;
            break;
//------------------------------------------------------------------------------
            case 'l':

                if ( (*(1+p)) == 'n') // функция LN - натур. логарифм
                {
                    temp->ThisIs = LN;
                    p += 2;
                }
                else
                    if ( (*(1+p)) == 'g') // функция lg - десятичный логарифм
                    {
                        temp->ThisIs = LG;
                        p += 2;
                    }
                    else
                        if ( (*(1+p)) == 'o' &&
                             (*(2+p)) == 'g')
                            // функция log - логарифм по произвольному основанию
                        {
                            temp->ThisIs = LOG;
                            p += 3;
                        }
            break;
//------------------------------------------------------------------------------
            case 'm':

                if ( (*(1+p)) == 'o' &&
                     (*(2+p)) == 'd' &&
                     (*(3+p)) == 'u' &&
                     (*(4+p)) == 'l' &&
                     (*(5+p)) == 'e') // функция модуля module
                {
                    temp->ThisIs = COMPLEX_MODULE;
                    p += 6;
                }
                else
                    if ( (*(1+p)) == 'o' &&
                         (*(2+p)) == 'd') // оператор mod
                    {
                        temp->ThisIs = MOD;
                        p += 3; // увеличить на число букв в названии функции
                    }
                    else
                        if (*p == 'm' &&
                            (*(1+p)) == 'p') // масса протона   mp
                        {
                            temp->number = k_mp;
                            temp->ThisIs = NUMBER;  // числовые костанты
                            p += 2;                 // названия констант длиной 2 символа
                        }
                        else
                            if (*p == 'm' &&
                               (*(1+p)) == 'e') // масса электрона me
                            {
                                temp->number = k_me;
                                temp->ThisIs = NUMBER;
                                p += 2;
                            }
                            else
                                if (*p == 'm' &&
                                    (*(1+p)) == 'n') // масса нейтрона mn
                                {
                                    temp->number = k_mn;
                                    temp->ThisIs = NUMBER;
                                    p += 2;
                                }
                                else
                                    if (*p == 'm' &&
                                        (*(1+p)) == 'o') // константа mo - "мю нулевое"
                                    {
                                        temp->number = k_m0;
                                        temp->ThisIs = NUMBER;
                                        p += 2;
                                    }
                                    else
                                        if ( (*(1+p)) == 'i' &&
                                             (*(2+p)) == 'd') // функция mid
                                        {
                                            temp->ThisIs = MIDDLE;
                                            p += 3;
                                        }
                                        else
                                            if ( (*(1+p)) == 'i' &&
                                                 (*(2+p)) == 'n') // функция min
                                            {
                                                temp->ThisIs = MIN2;
                                                p += 3;
                                            }
                                            else
                                                if ( (*(1+p)) == 'a' &&
                                                     (*(2+p)) == 'x') // функция max
                                                {
                                                    temp->ThisIs = MAX2;
                                                    p += 3;
                                                }
            break;
//------------------------------------------------------------------------------
            case 'N':
            case 'n':

                if ( *p == 'n' &&
                    (*(1+p)) == 'o' &&
                     (*(2+p)) == 'd') // функция nod
                {
                    temp->ThisIs = NOD_DEL;
                    p += 3;
                }
                else
                    if (*p == 'N' &&
                        (*(1+p)) == 'a') // число Авогадро Na
                    {
                        temp->number = k_Na;
                        p += 2;
                        temp->ThisIs = NUMBER;
                    }
                    else
                        if ( *p == 'n' &&
                            (*(1+p)) == 'o' &&
                             (*(2+p)) == 't') // оператор отрицания not
                        {
                            temp->ThisIs = NOT;
                            p += 3;
                        }
                        else
                            if ( *p == 'n' &&
                                (*(1+p)) == 'e' &&
                                (*(2+p)) == 'w') // функция nod
                            {
                                temp->ThisIs = NEW;
                                p += 3;
                            }
            break;
//------------------------------------------------------------------------------
            case 'o':

                if ( (*(1+p)) == 'r') // логический оператор or
                {
                    temp->ThisIs = OR;
                    p += 2;
                }
            break;
//------------------------------------------------------------------------------
            case 'p':

                if ( (*(1+p)) == 'o' &&
                     (*(2+p)) == 'w') // функция pow
                {
                    temp->ThisIs = POW;
                    p += 3;
                }
                else
                    if (*p == 'p' &&
                        (*(1+p)) == 'i') // константа pi = 3.14...
                    {
                        temp->number = M_PI;
                        p += 2;
                        temp->ThisIs = NUMBER;
                    }
            break;
//------------------------------------------------------------------------------
            case 'R':
            case 'r':

                if ( *p == 'r' &&
                    (*(1+p)) == 'o' &&
                     (*(2+p)) == 'u' &&
                     (*(3+p)) == 'n' &&
                     (*(4+p)) == 'd') // функция round
                {
                    temp->ThisIs = ROUND;
                    p += 5;
                }
                else
                    if ( *p == 'r' &&
                        (*(1+p)) == 'e' &&
                         (*(2+p)) == 't' &&
                         (*(3+p)) == 'u' &&
                         (*(4+p)) == 'r' &&
                         (*(5+p)) == 'n') // функция return
                    {
                        temp->ThisIs = RETURN;
                        p += 6;
                    }
                    else
                        if ( *p == 'r' &&
                            (*(1+p)) == 'a' &&
                             (*(2+p)) == 'n' &&
                             (*(3+p)) == 'd') // функция rand
                        {
                            temp->ThisIs = RAND;
                            p += 4;
                        }
                        else
                            if ( *p == 'r' &&
                                (*(1+p)) == 'u' &&
                                 (*(2+p)) == 'n')
                                // функция run вызова пользовательской функции
                            {
                                temp->ThisIs = RUN;
                                p += 3;
                            }
                            else
                                if ((*p) == 'r' &&
                                    (*(1+p)) == 'e')
                                    // идентификатор действительной части значения переменной re
                                {
                                    p += 2;
                                    temp->ThisIs = MEMBER_STRUCT_RE;
                                }
                                else
                                    if (*p == 'R') // универсальная газовая постоянная R
                                    {
                                        temp->ThisIs = NUMBER;
                                        temp->number = k_R;
                                        p++;
                                    }
            break;
//------------------------------------------------------------------------------
            case 's':

                if ( (*(1+p)) == 'i' &&
                     (*(2+p)) == 'n') // функция sin
                {
                    temp->ThisIs = SIN;
                    p += 3;
                }
                else
                    if ( (*(1+p)) == 'q' &&
                         (*(2+p)) == 'r' &&
                         (*(3+p)) == 't') // функция sqrt
                    {
                        temp->ThisIs = SQRT;
                        p += 4;
                    }
                    else
                        if ( (*(1+p)) == 'q' &&
                             (*(2+p)) == 'r') // функция sqr
                        {
                            temp->ThisIs = SQR;
                            p += 3;
                        }
                        else
                            if ( (*(1+p)) == 'h') // гиперболический синус sh
                            {
                                temp->ThisIs = SH_;
                                p += 2;
                            }
                            else
                                if ( (*(1+p)) == 'e' &&
                                     (*(2+p)) == 'c') // функция секанс sec
                                {
                                    temp->ThisIs = SEC;
                                    p += 3;
                                }
            break;
//------------------------------------------------------------------------------
            case 't':

                if ( (*(1+p)) == 'g') // функция tg
                {
                    temp->ThisIs = TG;
                    p += 2;
                }
                else
                    if ( (*(p+1)) == 'h') // гиперболический тангенс th
                    {
                        temp->ThisIs = TH;
                        p += 2;
                    }
                    else
                        if ( (*(p+1)) == 'o' &&
                             (*(p+2)) == 'r' &&
                             (*(p+3)) == 'a' &&
                             (*(p+4)) == 'd') // функция перевода в радианы torad
                        {
                            temp->ThisIs = TORAD;
                            p += 5;
                        }
                        else
                            if ( (*(p+1)) == 'o' &&
                                 (*(p+2)) == 'd' &&
                                 (*(p+3)) == 'e' &&
                                 (*(p+4)) == 'g')
                                // функция перевода в градусы todeg
                            {
                                temp->ThisIs = TODEG;
                                p += 5;
                            }
                            else
                                if ( (*(p+1)) == 'r' &&
                                     (*(p+2)) == 'u' &&
                                     (*(p+3)) == 'e')
                                    // логическая константа true
                                {
                                    temp->ThisIs = BOOL;
                                    temp->number = 1.;
                                    p += 4;
                                }
            break;
//------------------------------------------------------------------------------
            case 'V':

                if (*(1+p) == 'm') // молярный объем газа при н.у.
                {
                    temp->number = k_Vm;
                    temp->ThisIs = NUMBER;
                    p += 2;
                }
            break;
//------------------------------------------------------------------------------
            case 'w':

                if ( (*(p+1)) == 'r' &&
                     (*(p+2)) == 'i' &&
                     (*(p+3)) == 't' &&
                     (*(p+4)) == 'e' &&
                     (*(p+5)) == 'l' &&
                     (*(p+6)) == 'n') // функция writeln записи в выходной файл
                    // данных с дозаписью символа новой строки
                {
                    p += 7;
                    temp->ThisIs = WRITELN;
                }
                else
                    if ( (*(p+1)) == 'r' &&
                         (*(p+2)) == 'i' &&
                         (*(p+3)) == 't' &&
                         (*(p+4)) == 'e')
                        // функция writeln записи в выходной файл данных
                    {
                        p += 5;
                        temp->ThisIs = WRITE;
                    }
                    else
                        if ( (*(p+1)) == 'h' &&
                             (*(p+2)) == 'i' &&
                             (*(p+3)) == 'l' &&
                             (*(p+4)) == 'e') // оператор цикла while
                        {
                            p += 5;
                            temp->ThisIs = WHILE;
                        }
            break;
//------------------------------------------------------------------------------
            default: // число или неизвестная команда

                temp->ThisIs = NUMBER; // допустим, что число
                double temp_number;

                bool is_complex = false; // флаг определяет, комплексное это
                // число или вещественное

                if (*p == 'i') // это мнимая единица?
                {
                    ++p; // перейти к следующему символу
                    temp->Im_number = 1.;
                    goto add; // добавить найденную лексему в список
                }

                if (!isdigit(*p)) // если не число
                {
                    rprin("Неизвестная команда \"");
                    putchar(*p);
                    rprin("\" !");
                    error = true;
                    delete temp;
                    return;
                }

                temp_number = atof (p); // точно, что число
                while (((isdigit (*p)) || *p == '.') && *p++);
                // пропустить число в строке и десятичную точку

                if (*p) // если не достигли конца строки
                {
                    if (*p == 'e'||*p == 'E')
                        // если число записано в экспоненциальном виде
                    {
                        ++p; // пропустить экспоненту
                        if (*p)  // если не достигли конца строки
                            if (*p == '+' || *p == '-') // если достигли знака порядка числа после экспоненты, например 2е-3
                                p++; // пропустить знак
                        if (*p)  // если не достигли конца строки
                            while (isdigit (*p) && *p++); // пропустить пробелы в строке
                    }
                }
                if (*p == 'i') // если после числа стоит мнимая единица, например 2.5i
                {
                    is_complex = true;
                    ++p;
                }
                if (is_complex) // если найдено комплексное число
                    temp->Im_number = temp_number;
                else
                    temp->number = temp_number;

        } // switch (*p)

add:    AddInEndVersionShildt (temp, &end);
        // добавить найденную лексему в список лексем

        if (begin)          // если первая итерация цикла,
            start = temp, begin = false; // то инициализировать указатель начала списка

    } // for (iteration = 0; ; ++iteration) // Цикл заполнения списка лексем

    if (countBrecketsOpen != countBrecketsClose) // если скобки несбалансированы
    {
        rprin ("Не хватает ");

        // если открывающихся скобок больше закрывающихся
        if (countBrecketsOpen > countBrecketsClose)
        {
            // отобразить сколько не хватает нужных скобок
            printf ("%i", countBrecketsOpen - countBrecketsClose);
            rprin (" закрывающ(ейся)(ихся) скоб(ки)(ок)!");
        }
        else
        {
            printf ("%i", -countBrecketsOpen + countBrecketsClose);
            rprin (" открывающ(ейся)(ихся) скоб(ки)(ок)!");
        }
        error = true;
        return;
    }

    // если число модулей нечетно (т.к. |-3| = 3 - всегда четно)
    if (countModule % 2)
    {
        rprin ("Неправильно расставлены модули числа!");
        error = true;
    }
}


