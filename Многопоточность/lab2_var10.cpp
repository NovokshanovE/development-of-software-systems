#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <cstring>
#include <pthread.h>
#include <stdlib.h>
#include <semaphore.h>

const int FLOOR_COUNT = 10;  // Количество этажей

struct Lift  // Структура лифта
{
    int speed;  // Коэффициент скорости движения лифта, где 1 = 100000 мс
    int occupancy;  // Заполненность лифта
    int cur_floor;  // Нынешний этаж лифта
    int capacity;  // Максимальный объем лифта
    sem_t sem;  // Семафор лифта, отвечающий за блокировку лифта, когда нет сигнала о вызове
    char queue[FLOOR_COUNT * 2 + 1];  // Порядок остановки лифта на этажах
    char occupancy_mask[FLOOR_COUNT * 2 + 1];  // Показатель, сколько на каждом вызове хочет зайти или выйти людей

    Lift(int _speed, int _capacity): speed(_speed), occupancy(0), cur_floor(0), capacity(_capacity)  // Конструктор
    {
        for (int i = 0; i < FLOOR_COUNT * 2 + 1; i++)  // Больше, чем 2 * кол-во этажей вызовов быть не может, т. к. иначе вызовы идут в маску
        {
            queue[i] = '\0';  // Пустые элементы queue заполняются пустыми байтами для работы strlen(queue)
            occupancy_mask[i] = 0;
        }
        //sem_init(&sem, 1, 0);
    }

    void to_queue(int floor, int occupancy_change);  // Добавление в очередь запроса на вызов
    void move();  // Передвижение лифта ближе к этажу следующего вызова за определенное скоростью время
    void insert(int dest, char num, int occupancy_change);  // Вставка элемента по заданному индексу в массив порядка остановки
    void del(int dest);  // Удаление по заданному индексу элемента из массива порядки остановки

    int get_queue_time(int floor);  // Получение времени, требующегося лифту для добирания до заданного этажа
};

// dest - индекс вставки, num - какой этаж вставляем, occupancy_change - изменение кол-ва людей в лифте на этом этаже
void Lift::insert(int dest, char num, int occupancy_change)
{
    for (int i = strlen(queue); i > dest; i--)  // Просто сдвиг вправо
    {
        queue[i] = queue[i - 1];
        occupancy_mask[i] = occupancy_mask[i - 1];
    }
    queue[dest] = num;
    occupancy_mask[dest] = occupancy_change;
}

void Lift::del(int dest)  // dest - индекс удаления
{
    for (int i = dest; i < strlen(queue); i++)  // Сдвиг влево
    {
        queue[i] = queue[i + 1];
        occupancy_mask[i] = occupancy_mask[i + 1];
    }
    queue[strlen(queue)] = '\0';
    occupancy_mask[strlen(queue)] = 0;
}

void Lift::move()
{
    int TICK = 100000;
    if (strlen(queue) == 0)
    {
        //Тут он в состоянии блокировки
        //usleep(TICK);
    }
    else if (queue[0] == cur_floor + '0')  // Если пришли в следующий по очереди этаж - люди входят или выходят
    {
        if (occupancy_mask[0] >= 0)  // Внутри происходит проверка на вход не больше чем максимального и выход не больше чем минимального кол-ва людей в лифте
            occupancy += (occupancy_mask[0] > capacity - occupancy ? capacity - occupancy : occupancy_mask[0]);
        else
            occupancy += (occupancy_mask[0] < occupancy * (-1) ? occupancy * (-1) : occupancy_mask[0]);
        del(0);
        usleep(TICK);  // Люди входят или выходят TICK времени просто потому что я так решил
        sem_wait(&sem);  // Запрос выполнен, семафор -= 1. Если все запросы выполнились, поток блокируется
    }
    else if (queue[0] > cur_floor + '0')  // Поднимается к вызову
    {
        cur_floor += 1;
        usleep(TICK * speed);  // Ждет время передвижения, зависящее от скорости
    }
    else if (queue[0] < cur_floor + '0')  // Опускается к вызову
    {
        cur_floor -= 1;
        usleep(TICK * speed);  // Ждет время передвижения, зависящее от скорости
    }
}

void Lift::to_queue(int floor, int occupancy_change)  // floor - добавляемый этаж, occupancy_change - изменение кол-ва людей в лифте
{
    // Добавление сделано так, что лифт сначала поднимается до самого высокого этажа из очереди вызвавших, потом саускается до самого низкого
    // Таким образом не происходит "прыганий" вверх-вниз от этажа к этажу
    if (strlen(queue) == 0)  // Если в лифте нет людей - просто добавляет
    {
        insert(0, floor + '0', occupancy_change);
    }
    else if (floor >= cur_floor) //Если добавляемый этаж выше нынешнего, то добавляем перед первым в очереди большим, чем добавляемый
    {
        for (int i = 0; i < strlen(queue); i++)
            if (queue[i] >= floor + '0')
            {
                if (queue[i] > floor + '0')
                    insert(i, floor + '0', occupancy_change);
                else
                    occupancy_mask[i] += occupancy_change;
                return;
            }
        insert(strlen(queue), floor + '0', occupancy_change);
    }
    else //Если добавляемый этаж ниже нынешнего, то добавляем после первого в очереди большего с конца
    {
        for (int i = strlen(queue); i > 0; i--)
            if (queue[i - 1] >= floor + '0')
            {
                if (queue[i - 1] > floor + '0')
                    insert(i, floor + '0', occupancy_change);
                else
                    occupancy_mask[i] += occupancy_change;
                return;
            }
        insert(0, floor + '0', occupancy_change);
    }
}

int Lift::get_queue_time(int floor)  // floor - до какого этажа хотим получить время
{
    // Ищет место для вставки требуемого этажа аналогичным образом с добавлением в очередь запросов,
    // но дополнительно считает время на перемещение между этажами, учитывает время на высадку-посадку пассажиров на этажах
    int sum = 0, curr_floor = cur_floor;  // sum - суммарное время, curr_floor - "нынешний" этаж при подсчете
    if (floor >= cur_floor)
    {
        for (int i = 0; i < strlen(queue); i++)
        {
            if (queue[i] >= floor + '0')  // Ищет место для "вставки" этажа
                return (sum + abs(curr_floor - floor)) * speed + strlen(queue) - 1 - i;//Возвращяет сумму + время на высадку/посадку и доезд до заданного этажа
            sum += abs(curr_floor - queue[i]);  // Добавляет время перемещения до следующего запроса
            curr_floor = queue[i];  // Меняет "нынешний" этаж
        }
        return abs(curr_floor - floor) * speed;  //Добавил бы floor в конец очереди
    }
    else
    {
        int i_insert = 0;
        for (int i = strlen(queue); i > 0; i--)  // Ищет индекс, куда вставил бы запрос с конца очереди запросов
            if (queue[i - 1] >= floor + '0')
            {
                i_insert = i;
                break;
            }
        for (int i = 0; i < i_insert; i++)  // Считает сумму времени на перемещение между этажами в запросах до полученного индекса
        {
            sum += abs(curr_floor - queue[i]);
            curr_floor = queue[i];
        }
        return (abs(curr_floor - floor) + sum) * speed + i_insert;  // Возвращает сумму + время на посадку/высадку + время до заданного этажа
    }
}

Lift lp(1, 6), lg(2, 9);  // Создание лифтов, в грузовом максимум 9 человек, в пассажирском 6
int done = 0;

void display()  // Отображение нынешнего местонахождения лифтов
{
    char occupancy_lp[2], occupancy_lg[2];  // Сколько человек в лифтах, в виде строки чтобы было легче выводить с помощью write
    occupancy_lp[0] = lp.occupancy + '0';//Переводит количестко людей в лифтах в строки просто прибавляя '0', встает ограничение в макс. объеме людей в 9
    occupancy_lg[0] = lg.occupancy + '0';

    fflush(stdin);  // Чистит поток вывода, потому что в неканоническом режиме выводится какая-то чепуха

    // Выводит сообщение сначала для пассажирского, потом для грузового лифта
    write(1, "LP: ", 4);
    for (int i = 0; i < FLOOR_COUNT; i++)  // Проходит по этажам
        if (i == lp.cur_floor)  // Если на этом этаже лифт - выводит сколько в нем людей
        {
            write(1, occupancy_lp, 2);
            write(1, " ", 1);
        }
        else  // Иначе пишет черточку
            write(1, "- ", 2);
    write(1, "\tLG: ", 5);
    for (int i = 0; i < FLOOR_COUNT; i++)  // Аналагично пассажирскому лифту
        if (i == lg.cur_floor)
        {
            write(1, occupancy_lg, 2);
            write(1, " ", 1);
        }
        else
            write(1, "- ", 2);

    for (int i = 0; i < 4 * FLOOR_COUNT + 18; i++)  // Возвращает вывод в начало строки
        write(1, "\b", 1);

}

int right_command(char command, int symbols_used[])  // Проверка на то, что полученная команда правильная
{
    for (int i = 0; i < 30; i++)
        if (command == symbols_used[i])
            return 1;
    return 0;
}

void* lp_controller(void* arg_p)  // Функция потока пассажирского лифта
{
    sem_init(&(lp.sem), 1, 0);  // Инициализация семафора пассажирского лифта
    sem_wait(&(lp.sem));  // Перевод лифта в состояние ожидание запроса
    while (!done)  // Цикл до завершения цикла основной программы, отмечающимся done = 1
    {
        lp.move();  // Движения лифта, занимающие время
    }
    pthread_exit(0);  // Завершение выполнения потока
}

void* lg_controller(void* arg_p)  // Все аналогично функции потока пассажирского лифта, но с грузовым лифтом
{
    sem_init(&(lg.sem), 1, 0);
    sem_wait(&(lg.sem));
    while (!done)
    {
        lg.move();
    }
    pthread_exit(0);
}

void* visualizer(void* arg_p)  // Функция потока-визуализатора, который выводит на экран нынешнее положение лифтов
{
    while (!done)  // Пока не завершится цикл основной программы
    {
        usleep(100000);  // Время ожидания между выводами
        display();  // Функция вывода
    }
    write(1, "\n", 1);  // Перевод строки после окончания, остается последнее положение лифтов
    pthread_exit(0);  // Завершение выполнения потока
}

void dispatcher()  // Основная функция потока контрольного блока лифтов
{
    char command;

    int symbols_used[30] = {  // Разные команды для лифтов
        '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
        'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',
        'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';'
    };
    int floor_converter[121];  // Массив для перевода команд из символа в определенный этаж
    for (int i = 0; i < 30; i++) // Заполняет массив перевода, при этом если в десятке 0 - вызов на посадку, 1 - на высадку в ЛП, 2 - высадку в ЛГ
        floor_converter[symbols_used[i]] = i;

    // Блок перевода ввода в неканонический режим
    struct termios savetty;
    struct termios tty;

    if (!isatty(0)) {  // Проверка на то, что ввод из терминала
        fprintf(stderr, "stdin not terminal\n");
        exit(1);
    }

    tcgetattr(0, &tty);
    savetty = tty;
    tty.c_lflag &= ~(ICANON | ECHO | ISIG);
    tty.c_cc[VMIN] = 1;
    tcsetattr(0, TCSAFLUSH, &tty);
    // Конец блока перевода ввода в неканонический режим

    while (read(0, &command, 1))  // Пока может считать команду - идет по циклу
    {
        if (right_command(command, symbols_used))  // Если получили не верную команду - конец ввода
            command = floor_converter[command];
        else
            break;
        switch (command / 10)  // Определяет какого рода команду получили
        {
        case 0:  // Команда на вызов на посадку
            if (lp.get_queue_time(command % 10) <= lg.get_queue_time(command % 10))  // Если ЛП приедет раньше или за то же время, что ЛГ, то едет ЛП
            {
                sem_post(&(lp.sem));  // При получении команды, семафор ЛП += 1. Если до этого он был заблокирован - начинает работать
                lp.to_queue(command % 10, 1);  // Добавление нового запроса на перемещение к этажу
            }
            else
            {
                sem_post(&(lg.sem));  // Аналогично с ЛП
                lg.to_queue(command % 10, 1);
            }
            break;
        case 1:  // Команда на вызов на высадку из ЛП
            if (lp.occupancy > 0)  // Если людей в ЛП нет, то вызов не мог произвестись
            {
                sem_post(&(lp.sem));  // Новый запрос => семафор += 1
                lp.to_queue(command % 10, -1);  // Добавляет запрос в очередь
            }
            break;
        case 2:  // Команда на вызов на высадку из ЛГ, все аналогично с ЛП
            if (lg.occupancy > 0)
            {
                sem_post(&(lg.sem));
                lg.to_queue(command % 10, -1);
            }
            break;
        }
    }

    done = 1;  // Если цикл завершился - сообщает об этом через глобальную атомарную переменную done
    sem_post(&(lp.sem));  // Разблокирует поток ЛП 
    sem_post(&(lg.sem));  // Разблокирует поток ЛГ

    tcsetattr(0, TCSAFLUSH, &savetty);  // Переводит ввод в канонический режим
}

int main(int argc, char** argv)
{
    pthread_t tidp, tidg, tidv;  // tidp - поток ЛП, tidg - поток ЛГ, tidv - поток визуализатора
    pthread_attr_t pattr;

    // Инициализирует атрибуты потоков
    pthread_attr_init(&pattr);
    pthread_attr_setscope(&pattr, PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setdetachstate(&pattr, PTHREAD_CREATE_JOINABLE);

    // Создает потоки
    pthread_create(&tidp, &pattr, lp_controller, NULL);
    pthread_create(&tidg, &pattr, lg_controller, NULL);
    pthread_create(&tidv, &pattr, visualizer, NULL);

    dispatcher();  // Вызов функции потока блока контролля (родительского потока)

    // Родительский поток ждет завершения дочерних потоков
    pthread_join(tidp, NULL);
    pthread_join(tidg, NULL);
    pthread_join(tidv, NULL);

    // Очистка памати семафоров
    sem_destroy(&(lg.sem));
    sem_destroy(&(lg.sem));

    return 0; // Завершение программы
}