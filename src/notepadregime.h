#ifndef NOTEPADREGIME_H
#define NOTEPADREGIME_H

/////////////////////////////////////////////////////////////////////////////////////////////////////////
///     notepadregime.h - функции для работы калькулятора в режиме блокнота.
///
///                     ИНТЕРПРЕТАТОР 1.0
/////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "definitions.h"

//================ ПРОТОТИПЫ ФУНКЦИЙ =================================

// функция считывания выражения из файла исходного кода. Выражения отделены точкой с запятой.
// В параметрах - массив, в который запишется считанное выражение и указатель на файл.
// Комметарии всех типов и повторяющиеся символы пробела не записываются в строку.
// Возвращает строку с выражением.
char *FreadExpression (char *str, FILE *f);

// функция работы интерпретатора в режиме разбора текстовых файлов с исходным кодом.
// Функция запускает интерпретатор - пользователь набирает в текстовом
// файле неограниченое число выражений через ";" и закрывает его сохранив;
// программа считывает каждое выражение в текстовую строку, вычисляет его,
// и если нет ошибок, открывает для пользователя выходной файл; просмотрев
// и закрыв его, пользователь может повторить цикл работы.
void NotepadRegime ();

//================ ОПИСАНИЕ ФУНКЦИЙ ===================================

// Функция удаляет из памяти пользовательские массивы, хранящиеся в глобальном массиве
// ListArray, недоходя до индекса size.
// Применяется в случае обработки ошибки, когда флаг error установлен в истину -
// необходимо удалить их ListArray все пользовательские массивы, возможно искаженные
// из-за ошибки и затем другой функцией скопировать в ListArray данных из OLDListArray,
// хранящего резервную копию данных.
void DeleteAllArrays (int size);

//===============================================================
// Копирование size массивов из х1 в х2.
// Функция применяется перед разбором выражения, когда нужно сделать копию
// пользовательских массивов, чтобы в случае обнаружения ошибки в выражении можно было
// восстановить их до исходного состояния. Таким образом пользовательские данные будут
// защищены от нежелательных действий ошибки.
void CopyArrays (UserArray *x1, UserArray *x2, int size);

//===============================================================
// Функция удаляет из памяти пользовательские переменные, хранящиеся в глобальном массиве
// ListVar, недоходя до индекса size.
// Применяется в случае обработки ошибки, когда флаг error установлен в истину -
// необходимо удалить их ListVar все пользовательские переменные, возможно искаженные
// из-за ошибки и затем другой функцией скопировать в ListVar данных из OLDListVar,
// хранящего резервную копию данных.
void DeleteAllVar (int size);

//=============================================================
// Копирование size пользовательских переменных из х1 в х2.
// Функция применяется перед разбором выражения, когда нужно сделать копию
// пользовательских переменных, чтобы в случае обнаружения ошибки в выражении можно было
// восстановить их до исходного состояния. Таким образом пользовательские данные будут
// защищены от нежелательных действий ошибки.
void CopyVar (Variable *x1, Variable *x2, int size);

//=============================================================
// Функция удаляет из памяти пользовательские функции, хранящиеся в глобальном массиве
// ListFunctions, недоходя до индекса size.
// Применяется в случае обработки ошибки, когда флаг error установлен в истину -
// необходимо удалить их ListFunctions все пользовательские функции, возможно искаженные
// из-за ошибки и затем другой функцией скопировать в ListFunctions данных из
// OLDListFunctions, хранящего резервную копию данных.
void DeleteAllFunc (int size);

//=============================================================
// Копирование size функций из х1 в х2.
// Функция применяется перед разбором выражения, когда нужно сделать копию
// пользовательских функций, чтобы в случае обнаружения ошибки в выражении можно было
// восстановить их до исходного состояния. Таким образом пользовательские данные будут
// защищены от нежелательных действий ошибки.
void CopyFunc (Function *x1, Function *x2, int size);

//===============================================================
// функция удаляет строки в strings после индекса countStr. Используется после
// выполнения блока внутри фигурных скобок
void DeleteStrings (int countStr);

#endif // NOTEPADREGIME_H
