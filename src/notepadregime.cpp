
/////////////////////////////////////////////////////////////////////////////////////////////////////////
///     notepadregime.cpp - функции для работы калькулятора в режиме блокнота.
///
///                     ИНТЕРПРЕТАТОР 1.0
/////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "definitions.h"
#include "notepadregime.h"
#include "complex.h"
#include "interpreter.h"

#ifdef WINDOWS
extern char *FORMULA, *RESULT;
#endif

extern struct complex answer, // предыдущее вычисленное значение, определяет метку ans в
                        // строке-выражении, изначально равно 0

        return_value;

extern FILE *source, // файл с кодом программы
*result;  // файл для записи данных функциями write и writeln.

extern int count;  // счетчик. Используется при создании массивов.

extern struct UserArray // структура пользовательского массива
 *ListArray, // массив пользовательских массивов
  *OLDListArray; // резервная копия массивов на случай ошибки в выражении


extern struct Variable // структура пользовательской переменной
 *ListVar, // массив пользовательских переменных
  *OLDListVar;  // массив пользовательских переменных до вычисления выражения. Если в ходе
                // вычисления выражения нашлись ошибки, то массив переменных возвращается
                // в исходное состояние, что делается с целью отката нежелательных изменений
                // значений переменных, таким образом ошибка не повлияет на целостность
                // данных.


extern struct Function // структура пользовательской функции
      *ListFunctions, // Массив пользовательских функций
      *OLDListFunctions; // массив пользовательских функций до вычисления выражения.

extern char **strings;      // массив строк, заполняется в ходе выполнения выражения,
// а после удаляется

extern int count_recurs;   // счетчик рекурсивных вызовов пользовательской функции,
// необходим для реализации механизма рекурсии.

// функция считывания в str одного выражения из файла f. Выражения отделены точкой с запятой.
// Комметарии всех типов и повторяющиеся символы пробела не записываются в строку.
// Возвращает str.
char *FreadExpression (char *str, FILE *f)
{
    char c, // производится посимвольное чтение файла, в с помещаются символы
        *ptr = str; // указатель на строку

    // счетчики открывающихся и закрывающихся фигурных скобок, которыми ограничиваются
    // строки (ещё кавычками), внутри строк могут быть любые символы.
    int countOpen,
        countClose;

    if (feof (f)) // если указатель ссылается на конец файла
        return NULL;

    for (;;) // цикл считывания выражения
    {
get_symbol: if (ptr - str == MAX_EXPRESSION - 1)
        {
            //rprin ("Слишком длинное выражение, возможна ошибка! ");
            *ptr = 0;
            return 0;
        }

        fread (&c, sizeof(char), 1, f); // считать символ

        if ( feof(f) || c == ';') // если достигли конца файла или ;
            break;

        // если предыдущий считаный символ был пробелом, следующий пробел не записывать
        // в строку
        if (isspace(c) && isspace (*(ptr-1)))
        {
            continue;
        }
        else
        {
            *ptr++ = c; // записать символ в строку
        }

            if (c == '"') // считать строку до следующих кавычек
            {
                while (!feof (f) || c == '"')
                {
                    if (ptr - str == MAX_EXPRESSION - 1)
                    {
                       // rprin ("Слишком длинное выражение, возможна ошибка! ");
                        *ptr = 0;
                        return 0;
                    }

                    fread (&c, sizeof(char), 1, f);
                    *ptr++ = c;

                    // если найдены кавычки и перед ними нет '\', завершить считывание
                    // строки в кавычках
                    if (c == '"' && *(ptr-2) != '\\')
                        break;
                }
                continue;
            } // if (c == '"')

            if (c == '{') // считать строку, заключенную в фигурные скобки
            {
                countOpen = 1;
                countClose = 0;
                // считывать строку пока не достигнем конца файла или пока не
                // достигнем '}', пропустив все вложенные блоки фигурных скобок
                while (!feof (f) && countOpen != countClose)
                {
met1:               if (ptr - str == MAX_EXPRESSION - 1)
                    {
                        //rprin ("Слишком длинное выражение, возможна ошибка!\r");
                        *ptr = 0;
                        return 0;
                    }

                    fread (&c, sizeof(char), 1, f);
                    // повторяющиеся пробелы не записывать
                    if (isspace(c) && isspace (*(ptr-1)))
                    {
                        continue;
                    }
                    else
                    {
                        *ptr++ =c; // записать символ в строку
                    }
                    if (c == '/') // проверить, не комментарий ли это
                    {
                        fread (&c, sizeof(char), 1, f);

                        if (feof (f) || c == ';')
                            break;

                        *ptr++ = c;

                        if (c == '*') // пропустить комметарии типа /* */
                        {
                            ptr -= 2; // убрать из строки сочетание /*
                            *ptr++ = 32;
                            // вместо комментария вписать пробел, так как в выражении
                            // комментарии могут быть в качестве пробела и разделять лексемы

                            while (!feof (f))
                            {
                                fread (&c, sizeof(char), 1, f);
                                if (c == '*')
                                {
                                    if (feof(f))
                                    {
                                        *ptr = 0;
                                        return str;
                                    }
                                    fread (&c, sizeof(char), 1, f);
                                    if (feof(f))
                                    {
                                        *ptr = 0;
                                        return str;
                                    }
                                    if (c == '/') // если найдено сочетание */
                                    {
                                        goto met1;
                                    }
                                    else
                                // сместить указатель файла на 1 байт назад для случая /***/
                                    {
                                        fseek (f, -1, SEEK_CUR);
                                    }
                                }
                            }
                        } // if (c == '*')

                        if (c == '/') // если комментарий типа //
                        {// больше в строку ничего не записывать и найти конец комментария
                            ptr -= 2;*ptr++ = 32;
                            // убрать пару // и вставить пробел т.к. комментарии
                            // могут разделять лексемы

                            while (!feof (f) || ( c == 10 || c == 13))
                            {
                                fread (&c, sizeof(char), 1, f);

                                if ( c == 10 || c == 13) // дошли до конца комментария
                                {
                                    goto met1;
                                }
                            }
                            *ptr = 0;
                            return str;
                        } // if (c == '/') // если комментарий типа //

                    } // if (c == '/') // проверить, не комментарий ли это
                    if (c == '{')
                    {
                        ++countOpen;
                    }
                    else
                    {
                        if (c == '}')
                        {
                            ++countClose;
                        }
                    }
                } // while (!feof (f) && countOpen != countClose)
                continue;
            } // if (c == '{')

            if (c == '(') // считать блок, заключенный в круглые скобки (конструкция for)
            {
                countOpen = 1;
                countClose = 0;
                // считывать строку пока не достигнем конца файла или пока не
                // достигнем ')', пропустив все вложенные блоки фигурных скобок
                while (!feof (f) && countOpen != countClose)
                {
met2:               if (ptr - str == MAX_EXPRESSION - 1)
                    {
                        rprin ("Слишком длинное выражение, возможна ошибка!\r");
                        *ptr = 0;
                        return 0;
                    }

                    fread (&c, sizeof(char), 1, f);
                    if (isspace(c) && isspace (*(ptr-1)))
                    {
                        continue;
                    }
                    else
                    {
                        *ptr++ = c; // записать символ в строку
                    }
                    if (c == '/') // проверить, не комментарий ли это
                    {
                        fread (&c, sizeof(char), 1, f);

                        if (feof (f) || c == ';')
                            break;

                        *ptr++ = c;

                        if (c == '*') // пропустить комметарии типа /* */
                        {
                            ptr -= 2; // убрать из строки сочетание /*
                            *ptr++ = 32;
                            // вместо комментария вписать пробел, так как в выражении
                            // комментарии могут быть в качестве пробела и разделять лексемы

                            while (!feof (f))
                            {
                                fread (&c, sizeof(char), 1, f);
                                if (c == '*')
                                {
                                    if (feof(f))
                                    {
                                        *ptr = 0;
                                        return str;
                                    }
                                    fread (&c, sizeof(char), 1, f);
                                    if (feof(f))
                                    {
                                        *ptr = 0;
                                        return str;
                                    }
                                    if (c == '/') // если найдено сочетание */
                                    {
                                        goto met2;//goto get_symbol;
                                    }
                                    else // сместить указатель файла на 1 байт назад для случая /***/
                                    {
                                        fseek (f, -1, SEEK_CUR);
                                    }
                                }
                            }
                        } // if (c == '*')

                        if (c == '/') // если комментарий типа //
                        {// больше в строку ничего не записывать и найти конец комментария
                            ptr -= 2;*ptr++ = 32;
                            // убрать пару // и вставить пробел т.к. комментарии
                            // могут разделять лексемы

                            while (!feof (f) || ( c == 10 || c == 13))
                            {
                                fread (&c, sizeof(char), 1, f);

                                if ( c == 10 || c == 13) // дошли до конца комментария
                                {
                                    goto met2;
                                }
                            }
                            *ptr = 0;
                            return str;
                        } // if (c == '/') // если комментарий типа //

                    } // if (c == '/') // проверить, не комментарий ли это
                    if (c == '(')
                    {
                        ++countOpen;
                    }
                    else
                    {
                        if (c == ')')
                        {
                            ++countClose;
                        }
                    }
                } // while (!feof (f) && countOpen != countClose)
                continue;
            } // if (c == '(')

        if (c == '/') // проверить, не комментарий ли это
        {
            fread (&c, sizeof(char), 1, f);

            if (feof (f) || c == ';')
                break;

            *ptr++ = c;

            if (c == '*') // пропустить комметарии типа /* */
            {
                ptr -= 2; // убрать из строки сочетание /*
                *ptr++ = 32;
                // вместо комментария вписать пробел, так как в выражении
                // комментарии могут быть в качестве пробела и разделять лексемы

                while (!feof (f))
                {
                    fread (&c, sizeof(char), 1, f);//putchar(c);
                    if (c == '*')
                    {
                        if (feof(f))
                        {
                            *ptr = 0;
                            return str;
                        }
                        fread (&c, sizeof(char), 1, f);//putchar(c);
                        if (feof(f))
                        {
                            *ptr = 0;
                            return str;
                        }
                        if (c == '/') // если найдено сочетание */
                        {
                            //puts("\nyes");
                            goto get_symbol;
                        }
                        else // сместить указатель файла на 1 байт назад для случая /***/
                        {
                            fseek (f, -1, SEEK_CUR);
                        }
                    }
                }
            } // if (c == '*')

            if (c == '/') // если комментарий типа //
            {// больше в строку ничего не записывать и найти конец комментария
                ptr -= 2;*ptr++ = 32; // убрать пару // и вставить пробел т.к. комментарии
                // могут разделять лексемы

                while (!feof (f) || ( c == 10 || c == 13))
                {
                    fread (&c, sizeof(char), 1, f);

                    if ( c == 10 || c == 13) // дошли до конца комментария
                    {
                        goto get_symbol;
                    }
                }
                *ptr = 0;
                return str;
            } // if (c == '/') // если комментарий типа //

        } // if (c == '/') // проверить, не комментарий ли это
    } // for (;;) // цикл считывания выражения
    *ptr = '\0';
    return str;
}
//===============================================================
// Функция удаляет из памяти пользовательские массивы, хранящиеся в глобальном массиве
// ListArray, недоходя до индекса size.
// Применяется в случае обработки ошибки, когда флаг error установлен в истину -
// необходимо удалить их ListArray все пользовательские массивы, возможно искаженные
// из-за ошибки и затем другой функцией скопировать в ListArray данных из OLDListArray,
// хранящего резервную копию данных.
void DeleteAllArrays (int size)
{
    // просмотреть массив ListArray
    for (int i=0; i<size; i++)
    {
        // обнулить размерность и имя пользовательского массива
        ListArray[i].dim = ListArray[i].nameArray[0] = 0;
        // если правильно указан адрес, удалить из памяти пользовательский массив
        if (ListArray[i].elements) delete [](ListArray[i].elements);
    }
    // удалить весь ListArray
    delete []ListArray;
    // выделить заново память для ListArray
    ListArray   = new UserArray [MAX_QUANTITY_USER_ARRAY];
    if (!ListArray)
    {
        // вывести сообщение об ошибке и завершить приложение
        FatalError ("Ошибка выделения памяти");
    }
}
//===============================================================
// Копирование size массивов из х1 в х2.
// Функция применяется перед разбором выражения, когда нужно сделать копию
// пользовательских массивов, чтобы в случае обнаружения ошибки в выражении можно было
// восстановить их до исходного состояния. Таким образом пользовательские данные будут
// защищены от нежелательных действий ошибки.
void CopyArrays (UserArray *x1, UserArray *x2, int size)
{
    complex *tmp; // указатель на пользовательский массив
    for (int i=0; i<size; i++) // цикл копирования массивов
    {
        if (x2[i].elements) // удалить пользовательский массив из х2 если он имеется
            delete [](x2[i].elements);

        if (x1[i].dim==1) // если одномерный массив
        {
            // выделить память под пользовательский массив
            tmp = new complex [x1[i].j];
            if (!tmp)
            {
                // вывести сообщение об ошибке и завершить приложение
                FatalError ("Ошибка выделения памяти");
            }
            // скопировать в tmp все элементы массива
            for(count=0; count<x1[i].j; tmp[count] = x1[i].elements[count++]);
        }
        else // иначе двумерный массив
        {
            // выделить память под пользовательский массив, число элементов в нем равно
            // число строк * на число стобцов: i*j
            tmp = new complex [x1[i].j*x1[i].i];
            if (!tmp)
            {
                // вывести сообщение об ошибке и завершить приложение
                FatalError ("Ошибка выделения памяти");
            }
            // скопировать в tmp все элементы массива
            for(count=0; count<x1[i].j*x1[i].i; tmp[count] = x1[i].elements[count++]);
        }

        // скопировать прочую информацию (размерность dim и т.д.)
        x2[i] = x1[i];
        // переопределить в х2 указаетель на пользовательский массив, теперь в текущем
        // элементе х2 хранится точная копия пользовательского массива
        x2[i].elements = tmp;
    }
}
//===============================================================
// Функция удаляет из памяти пользовательские переменные, хранящиеся в глобальном массиве
// ListVar, недоходя до индекса size.
// Применяется в случае обработки ошибки, когда флаг error установлен в истину -
// необходимо удалить их ListVar все пользовательские переменные, возможно искаженные
// из-за ошибки и затем другой функцией скопировать в ListVar данных из OLDListVar,
// хранящего резервную копию данных.
void DeleteAllVar (int size)
{
    delete []ListVar;
    ListVar = new Variable [QUANTITY_VARIABLES];
    if (!ListVar)
    {
        FatalError ("Ошибка выделения памяти"); // вывести сообщение об ошибке
    }
}
//=============================================================
// Копирование size пользовательских переменных из х1 в х2.
// Функция применяется перед разбором выражения, когда нужно сделать копию
// пользовательских переменных, чтобы в случае обнаружения ошибки в выражении можно было
// восстановить их до исходного состояния. Таким образом пользовательские данные будут
// защищены от нежелательных действий ошибки.
void CopyVar (Variable *x1, Variable *x2, int size)
{
    for (int i=0; i<size; i++)
    {
        x2[i] = x1[i];
    }
}
//=============================================================
// Функция удаляет из памяти пользовательские функции, хранящиеся в глобальном массиве
// ListFunctions, недоходя до индекса size.
// Применяется в случае обработки ошибки, когда флаг error установлен в истину -
// необходимо удалить их ListFunctions все пользовательские функции, возможно искаженные
// из-за ошибки и затем другой функцией скопировать в ListFunctions данных из
// OLDListFunctions, хранящего резервную копию данных.
void DeleteAllFunc (int size)
{
    // просмотреть массив ListFunctions
    for (int i=0; i<size; i++)
    {
        // обнулить имя пользовательской функции
        ListFunctions[i].name[0] = 0;
        // если правильно указан адрес, удалить из памяти пользовательскую функцию
        if (ListFunctions[i].body)
            delete [](ListFunctions[i].body);
    }
    // удалить весь ListFunctions
    delete []ListFunctions;
    // выделить заново память для ListFunctions
    ListFunctions   = new Function [MAX_QUANTITY_USER_FUNCTION];
    if (!ListFunctions)
    {
        FatalError ("Ошибка выделения памяти"); // вывести сообщение об ошибке
    }
}
//=============================================================
// Копирование size функций из х1 в х2.
// Функция применяется перед разбором выражения, когда нужно сделать копию
// пользовательских функций, чтобы в случае обнаружения ошибки в выражении можно было
// восстановить их до исходного состояния. Таким образом пользовательские данные будут
// защищены от нежелательных действий ошибки.
void CopyFunc (Function *x1, Function *x2, int size)
{
    char *tmp; // указатель на имя пользовательской функции
    for (int i=0; i<size; i++) // цикл копирования функций
    {
        // удалить имя пользовательской функции из х2 если адрес указан правильно
        if (x2[i].body)
            delete [](x2[i].body);

        // выделить память под имя пользовательской функции
        tmp = new char [MAX_STRING];
        if (!tmp)
        {
            FatalError ("Ошибка выделения памяти"); // вывести сообщение об ошибке
        }
        // скопировать в tmp имя пользовательской функции
        strcpy (tmp, x1[i].body);
        // скопировать в х2 прочую информацию
        x2[i] = x1[i];
        // переопределить в х2 указаетель на имя пользовательской функции, теперь в текущем
        // элементе х2 хранится точная копия пользовательской функции
        x2[i].body = tmp;
    }
}
//===============================================================
// Функция запускает калькулятор в режиме блокнота - пользователь набирает в текстовом
// файле неограниченое число выражений через ";" и закрывает его сохранив;
// программа считывает каждое выражение в текстовую строку, вычисляет его,
// и если нет ошибок, открывает для пользователя выходной файл; просмотрев
// и закрыв его, пользователь может повторить цикл калькулятора.
void NotepadRegime ()
{
    Interpreter ariph; // главный объект
    srand ((unsigned) time (NULL)); // инициализация генератора случайных чисел,
    // используется для выполнения встроенной функции калькулятора rand

    int CountExpressions;   // счетчик считанных выражений из файла кода
    int OLDcountFunction;   // число пользовательских функций до выполнения программы
    // из файла Код.txt.

    char *str; // строка с выражением
    str = new char [MAX_EXPRESSION];
    if (!str)
    {
        FatalError ("Ошибка выделения памяти"); // вывести сообщение об ошибке
    }

    struct complex current_result; // текущий результат вычисления выражения

    // вывести на консоль заголовок
    rputs ("\tИНТЕРПРЕТАТОР 1.0\n\n\
В файл \"Код.txt\" введите код программы и закройте редактор.\n\
В файле \"Результат.txt\" отобразится результат.\n\n");

    int CountArrays     = 0, // общее число созданных пользовательских массивов
        OLDCountArrays  = 0, // число пользовательских массивов до разбора строки
        CountVar        = 0, // общее число созданных пользовательских переменных
        OLDCountVar     = 0, // число пользовательских переменных до разбора строки
        CountFunc       = 0, // общее число созданных пользовательских функций
        OLDCountFunc    = 0; // число пользовательских функций до разбора строки

    // данные переменные хранят число созданных пользовательских объектов до разбора строки
    // и после. Если в ходе разбора строки обнаружена ошибка, только что созданные объекты
    // удаляются.

    // выделить память под массивы пользовательских объектов
    ListArray           = new UserArray [MAX_QUANTITY_USER_ARRAY];
    OLDListArray        = new UserArray [MAX_QUANTITY_USER_ARRAY];
    ListVar             = new Variable [QUANTITY_VARIABLES];
    OLDListVar          = new Variable [QUANTITY_VARIABLES];
    ListFunctions       = new Function [MAX_QUANTITY_USER_FUNCTION];
    OLDListFunctions    = new Function [MAX_QUANTITY_USER_FUNCTION];
    strings             = (char **) calloc (QUANTITY_STRINGS, sizeof (char*));
    // выделить память под указатели на строки

    if (!ListArray ||
        !OLDListArray ||
        !ListVar ||
        !OLDListVar ||
        !ListFunctions ||
        !OLDListFunctions ||
        !strings)
    {
        FatalError ("Ошибка выделения памяти"); // вывести сообщение об ошибке
    }

    // обнулить указатели на строки
    for (int i=0; i <QUANTITY_STRINGS; strings[i++] = 0);

    for (;;) // главный цикл работы интерпретатора
    {
        // если файл Код.txt не существует, создать его, иначе оставить его содержимое
        // для редактирования пользователем
        if (!EXISTS ("Код.txt"))
        {
            if (!(source = fopen_ ("Код.txt", "w")))
            {
                rprin ("\r                                                                           \r");
                // завершить приложение с выводом сообщения на экран
                FatalError ("Ошибка при открытии файла \"Код.txt\".");
            }            
            fclose (source);
        }
loop:
        // если файл Результат.txt существует, содержимое его уничтожается
        if (!(result = fopen_ ("Результат.txt", "wt")))
        {
            fclose (source);
            rprin ("\r                                                                             \r");
            FatalError ("Ошибка при открытии файла \"Результат.txt\".");
        }

        if (system (SOURCE)) // открыть файл для редактирования пользователем
        {
            fclose (source);
            FatalError ("Не обнаружен текстовый редактор для открытии файла \"Код.txt\".");
        }
            if (!(source = fopen_ ("Код.txt", "r+t")))
        {
            fclose (result);
#ifdef WINDOWS
            char *ptemp = get_windows1251("Результат.txt");
            remove (ptemp);
            delete ptemp;
#else
            remove ("Результат.txt");
#endif
            rprin ("\r                                                                             \r");
            FatalError ("Ошибка при открытии файла \"Код.txt\".");
        }

        error = Write = false; // false означает, что нет ошибок в выражениях и
        // записей в файл Результат.txt не производилось

        // обнулить счетчик считанных выражений
        CountExpressions = 0;

        // записать исходно число пользовательских
        // функций в главном объекте до цикла последовательного считывания и
        // выполнения выражений чтобы в случае обнаружения ошибки знать, сколько новых
        // объявленных функций удалить из памяти
        OLDcountFunction = OLDCountFunc = ariph.countFunctions;
        // сохранить в OLDListFunctions резервную копию данных
        CopyFunc (ListFunctions, OLDListFunctions, OLDCountFunc);

        OLDCountArrays = ariph.countArray;
        CopyArrays (ListArray, OLDListArray, OLDCountArrays);
        OLDCountVar = ariph.countVariables;
        CopyVar (ListVar, OLDListVar, OLDCountVar);

        for (;;) // цикл считывания и выполнения выражений
        {
            rprin ("\r                                                                             \r\tOK\r");
            // считать выражение. Если достигли конца файла Код.txt или файл пуст,
            // завершить текущий цикл
            if (!FreadExpression (str, source))
            {
                break;
            }


#ifdef LINUX
            char *ptemp = get_windows1251(str);
            strcpy(str, ptemp);
            delete ptemp;
#endif
            //puts("");
            //rputs(str);
            return_func = run_continue = run_break = false; // флаги показывают,
            // встретились ли в выражении команды return, continue и break вне тела
            // пользовательской функции

            // обнулить счетчики рекурсивных вызовов пользовательских функций и вложенности
            // циклов
            count_recurs = count_loop = ariph.countString = 0;

            // записать в current_result результат вычисления текущего выражения
            current_result = ariph.interpret (str);

            ++CountExpressions; // счетчик выражений. Используется чтобы показать номер
            // первого в программе выражения, в котором нашлась ошибка

            // удалить из памяти все строки созданные в ходе разбора выражения
            DeleteStrings (0);

            if (!error && !ThisComment) // если выражение не пустое и без ошибок
            {
                // записать в answer ответ. Так как в выражении может встретиться
                // метка ans, результат предыдущего выраения берется из переменной answer.
                answer = (return_func) ? return_value : current_result;
            }
            else
                if (error) // если же нашлась ошибка
                {
                    // отобразить номер ошибочного выражения
                    printf(" - expression %i", CountExpressions);
                    fclose (source);
                    fclose (result);

                    // удалить из объекта ariph все только что созданные
                    // пользовательские функции
                    ariph.countFunctions = OLDcountFunction;
                    DeleteAllFunc (CountFunc);
                    // восстановить исходное состояние пользовательских данных, т.е.
                    // отменить все переопределения функций и создание новых
                    CopyFunc (OLDListFunctions, ListFunctions, OLDCountFunc);
                    ariph.countFunctions  = OLDCountFunc;

                    DeleteAllArrays (CountArrays);
                    CopyArrays (OLDListArray, ListArray, OLDCountArrays);
                    ariph.countArray  = OLDCountArrays;
                    DeleteAllVar (CountVar);
                    CopyVar (OLDListVar, ListVar, OLDCountVar);
                    ariph.countVariables  = OLDCountVar;

                    goto loop;
                }

        } // for (;;) цикл считывания и выполнения выражений

        fclose (source);
        fclose (result);

        // если в файл Результат.txt записывались данные, открыть его для пользователя
        if (Write)
        {
            if (system (RESULT))
            {
                FatalError ("Не обнаружен текстовый редактор для открытии файла \"Результат.txt\".");
            }
        }
        else
            rprin ("Нет данных для отображения\r");

        //remove ("Код.txt");

    } // for (;;) // главный цикл работы калькулятора

    delete []str;
}

//===============================================================
void DeleteStrings (int countStr)
{
    while (strings[countStr])
    {
        delete[]strings[countStr];
        strings[countStr++]=0;
    }
}

