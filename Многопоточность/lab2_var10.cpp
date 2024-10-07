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

const int FLOOR_COUNT = 10;  // ���������� ������

struct Lift  // ��������� �����
{
    int speed;  // ����������� �������� �������� �����, ��� 1 = 100000 ��
    int occupancy;  // ������������� �����
    int cur_floor;  // �������� ���� �����
    int capacity;  // ������������ ����� �����
    sem_t sem;  // ������� �����, ���������� �� ���������� �����, ����� ��� ������� � ������
    char queue[FLOOR_COUNT * 2 + 1];  // ������� ��������� ����� �� ������
    char occupancy_mask[FLOOR_COUNT * 2 + 1];  // ����������, ������� �� ������ ������ ����� ����� ��� ����� �����

    Lift(int _speed, int _capacity): speed(_speed), occupancy(0), cur_floor(0), capacity(_capacity)  // �����������
    {
        for (int i = 0; i < FLOOR_COUNT * 2 + 1; i++)  // ������, ��� 2 * ���-�� ������ ������� ���� �� �����, �. �. ����� ������ ���� � �����
        {
            queue[i] = '\0';  // ������ �������� queue ����������� ������� ������� ��� ������ strlen(queue)
            occupancy_mask[i] = 0;
        }
        //sem_init(&sem, 1, 0);
    }

    void to_queue(int floor, int occupancy_change);  // ���������� � ������� ������� �� �����
    void move();  // ������������ ����� ����� � ����� ���������� ������ �� ������������ ��������� �����
    void insert(int dest, char num, int occupancy_change);  // ������� �������� �� ��������� ������� � ������ ������� ���������
    void del(int dest);  // �������� �� ��������� ������� �������� �� ������� ������� ���������

    int get_queue_time(int floor);  // ��������� �������, ������������ ����� ��� ��������� �� ��������� �����
};

// dest - ������ �������, num - ����� ���� ���������, occupancy_change - ��������� ���-�� ����� � ����� �� ���� �����
void Lift::insert(int dest, char num, int occupancy_change)
{
    for (int i = strlen(queue); i > dest; i--)  // ������ ����� ������
    {
        queue[i] = queue[i - 1];
        occupancy_mask[i] = occupancy_mask[i - 1];
    }
    queue[dest] = num;
    occupancy_mask[dest] = occupancy_change;
}

void Lift::del(int dest)  // dest - ������ ��������
{
    for (int i = dest; i < strlen(queue); i++)  // ����� �����
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
        //��� �� � ��������� ����������
        //usleep(TICK);
    }
    else if (queue[0] == cur_floor + '0')  // ���� ������ � ��������� �� ������� ���� - ���� ������ ��� �������
    {
        if (occupancy_mask[0] >= 0)  // ������ ���������� �������� �� ���� �� ������ ��� ������������� � ����� �� ������ ��� ������������ ���-�� ����� � �����
            occupancy += (occupancy_mask[0] > capacity - occupancy ? capacity - occupancy : occupancy_mask[0]);
        else
            occupancy += (occupancy_mask[0] < occupancy * (-1) ? occupancy * (-1) : occupancy_mask[0]);
        del(0);
        usleep(TICK);  // ���� ������ ��� ������� TICK ������� ������ ������ ��� � ��� �����
        sem_wait(&sem);  // ������ ��������, ������� -= 1. ���� ��� ������� �����������, ����� �����������
    }
    else if (queue[0] > cur_floor + '0')  // ����������� � ������
    {
        cur_floor += 1;
        usleep(TICK * speed);  // ���� ����� ������������, ��������� �� ��������
    }
    else if (queue[0] < cur_floor + '0')  // ���������� � ������
    {
        cur_floor -= 1;
        usleep(TICK * speed);  // ���� ����� ������������, ��������� �� ��������
    }
}

void Lift::to_queue(int floor, int occupancy_change)  // floor - ����������� ����, occupancy_change - ��������� ���-�� ����� � �����
{
    // ���������� ������� ���, ��� ���� ������� ����������� �� ������ �������� ����� �� ������� ���������, ����� ���������� �� ������ �������
    // ����� ������� �� ���������� "��������" �����-���� �� ����� � �����
    if (strlen(queue) == 0)  // ���� � ����� ��� ����� - ������ ���������
    {
        insert(0, floor + '0', occupancy_change);
    }
    else if (floor >= cur_floor) //���� ����������� ���� ���� ���������, �� ��������� ����� ������ � ������� �������, ��� �����������
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
    else //���� ����������� ���� ���� ���������, �� ��������� ����� ������� � ������� �������� � �����
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

int Lift::get_queue_time(int floor)  // floor - �� ������ ����� ����� �������� �����
{
    // ���� ����� ��� ������� ���������� ����� ����������� ������� � ����������� � ������� ��������,
    // �� ������������� ������� ����� �� ����������� ����� �������, ��������� ����� �� �������-������� ���������� �� ������
    int sum = 0, curr_floor = cur_floor;  // sum - ��������� �����, curr_floor - "��������" ���� ��� ��������
    if (floor >= cur_floor)
    {
        for (int i = 0; i < strlen(queue); i++)
        {
            if (queue[i] >= floor + '0')  // ���� ����� ��� "�������" �����
                return (sum + abs(curr_floor - floor)) * speed + strlen(queue) - 1 - i;//���������� ����� + ����� �� �������/������� � ����� �� ��������� �����
            sum += abs(curr_floor - queue[i]);  // ��������� ����� ����������� �� ���������� �������
            curr_floor = queue[i];  // ������ "��������" ����
        }
        return abs(curr_floor - floor) * speed;  //������� �� floor � ����� �������
    }
    else
    {
        int i_insert = 0;
        for (int i = strlen(queue); i > 0; i--)  // ���� ������, ���� ������� �� ������ � ����� ������� ��������
            if (queue[i - 1] >= floor + '0')
            {
                i_insert = i;
                break;
            }
        for (int i = 0; i < i_insert; i++)  // ������� ����� ������� �� ����������� ����� ������� � �������� �� ����������� �������
        {
            sum += abs(curr_floor - queue[i]);
            curr_floor = queue[i];
        }
        return (abs(curr_floor - floor) + sum) * speed + i_insert;  // ���������� ����� + ����� �� �������/������� + ����� �� ��������� �����
    }
}

Lift lp(1, 6), lg(2, 9);  // �������� ������, � �������� �������� 9 �������, � ������������ 6
int done = 0;

void display()  // ����������� ��������� ��������������� ������
{
    char occupancy_lp[2], occupancy_lg[2];  // ������� ������� � ������, � ���� ������ ����� ���� ����� �������� � ������� write
    occupancy_lp[0] = lp.occupancy + '0';//��������� ���������� ����� � ������ � ������ ������ ��������� '0', ������ ����������� � ����. ������ ����� � 9
    occupancy_lg[0] = lg.occupancy + '0';

    fflush(stdin);  // ������ ����� ������, ������ ��� � �������������� ������ ��������� �����-�� ������

    // ������� ��������� ������� ��� �������������, ����� ��� ��������� �����
    write(1, "LP: ", 4);
    for (int i = 0; i < FLOOR_COUNT; i++)  // �������� �� ������
        if (i == lp.cur_floor)  // ���� �� ���� ����� ���� - ������� ������� � ��� �����
        {
            write(1, occupancy_lp, 2);
            write(1, " ", 1);
        }
        else  // ����� ����� ��������
            write(1, "- ", 2);
    write(1, "\tLG: ", 5);
    for (int i = 0; i < FLOOR_COUNT; i++)  // ���������� ������������� �����
        if (i == lg.cur_floor)
        {
            write(1, occupancy_lg, 2);
            write(1, " ", 1);
        }
        else
            write(1, "- ", 2);

    for (int i = 0; i < 4 * FLOOR_COUNT + 18; i++)  // ���������� ����� � ������ ������
        write(1, "\b", 1);

}

int right_command(char command, int symbols_used[])  // �������� �� ��, ��� ���������� ������� ����������
{
    for (int i = 0; i < 30; i++)
        if (command == symbols_used[i])
            return 1;
    return 0;
}

void* lp_controller(void* arg_p)  // ������� ������ ������������� �����
{
    sem_init(&(lp.sem), 1, 0);  // ������������� �������� ������������� �����
    sem_wait(&(lp.sem));  // ������� ����� � ��������� �������� �������
    while (!done)  // ���� �� ���������� ����� �������� ���������, ������������ done = 1
    {
        lp.move();  // �������� �����, ���������� �����
    }
    pthread_exit(0);  // ���������� ���������� ������
}

void* lg_controller(void* arg_p)  // ��� ���������� ������� ������ ������������� �����, �� � �������� ������
{
    sem_init(&(lg.sem), 1, 0);
    sem_wait(&(lg.sem));
    while (!done)
    {
        lg.move();
    }
    pthread_exit(0);
}

void* visualizer(void* arg_p)  // ������� ������-�������������, ������� ������� �� ����� �������� ��������� ������
{
    while (!done)  // ���� �� ���������� ���� �������� ���������
    {
        usleep(100000);  // ����� �������� ����� ��������
        display();  // ������� ������
    }
    write(1, "\n", 1);  // ������� ������ ����� ���������, �������� ��������� ��������� ������
    pthread_exit(0);  // ���������� ���������� ������
}

void dispatcher()  // �������� ������� ������ ������������ ����� ������
{
    char command;

    int symbols_used[30] = {  // ������ ������� ��� ������
        '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
        'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',
        'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';'
    };
    int floor_converter[121];  // ������ ��� �������� ������ �� ������� � ������������ ����
    for (int i = 0; i < 30; i++) // ��������� ������ ��������, ��� ���� ���� � ������� 0 - ����� �� �������, 1 - �� ������� � ��, 2 - ������� � ��
        floor_converter[symbols_used[i]] = i;

    // ���� �������� ����� � �������������� �����
    struct termios savetty;
    struct termios tty;

    if (!isatty(0)) {  // �������� �� ��, ��� ���� �� ���������
        fprintf(stderr, "stdin not terminal\n");
        exit(1);
    }

    tcgetattr(0, &tty);
    savetty = tty;
    tty.c_lflag &= ~(ICANON | ECHO | ISIG);
    tty.c_cc[VMIN] = 1;
    tcsetattr(0, TCSAFLUSH, &tty);
    // ����� ����� �������� ����� � �������������� �����

    while (read(0, &command, 1))  // ���� ����� ������� ������� - ���� �� �����
    {
        if (right_command(command, symbols_used))  // ���� �������� �� ������ ������� - ����� �����
            command = floor_converter[command];
        else
            break;
        switch (command / 10)  // ���������� ������ ���� ������� ��������
        {
        case 0:  // ������� �� ����� �� �������
            if (lp.get_queue_time(command % 10) <= lg.get_queue_time(command % 10))  // ���� �� ������� ������ ��� �� �� �� �����, ��� ��, �� ���� ��
            {
                sem_post(&(lp.sem));  // ��� ��������� �������, ������� �� += 1. ���� �� ����� �� ��� ������������ - �������� ��������
                lp.to_queue(command % 10, 1);  // ���������� ������ ������� �� ����������� � �����
            }
            else
            {
                sem_post(&(lg.sem));  // ���������� � ��
                lg.to_queue(command % 10, 1);
            }
            break;
        case 1:  // ������� �� ����� �� ������� �� ��
            if (lp.occupancy > 0)  // ���� ����� � �� ���, �� ����� �� ��� ������������
            {
                sem_post(&(lp.sem));  // ����� ������ => ������� += 1
                lp.to_queue(command % 10, -1);  // ��������� ������ � �������
            }
            break;
        case 2:  // ������� �� ����� �� ������� �� ��, ��� ���������� � ��
            if (lg.occupancy > 0)
            {
                sem_post(&(lg.sem));
                lg.to_queue(command % 10, -1);
            }
            break;
        }
    }

    done = 1;  // ���� ���� ���������� - �������� �� ���� ����� ���������� ��������� ���������� done
    sem_post(&(lp.sem));  // ������������ ����� �� 
    sem_post(&(lg.sem));  // ������������ ����� ��

    tcsetattr(0, TCSAFLUSH, &savetty);  // ��������� ���� � ������������ �����
}

int main(int argc, char** argv)
{
    pthread_t tidp, tidg, tidv;  // tidp - ����� ��, tidg - ����� ��, tidv - ����� �������������
    pthread_attr_t pattr;

    // �������������� �������� �������
    pthread_attr_init(&pattr);
    pthread_attr_setscope(&pattr, PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setdetachstate(&pattr, PTHREAD_CREATE_JOINABLE);

    // ������� ������
    pthread_create(&tidp, &pattr, lp_controller, NULL);
    pthread_create(&tidg, &pattr, lg_controller, NULL);
    pthread_create(&tidv, &pattr, visualizer, NULL);

    dispatcher();  // ����� ������� ������ ����� ��������� (������������� ������)

    // ������������ ����� ���� ���������� �������� �������
    pthread_join(tidp, NULL);
    pthread_join(tidg, NULL);
    pthread_join(tidv, NULL);

    // ������� ������ ���������
    sem_destroy(&(lg.sem));
    sem_destroy(&(lg.sem));

    return 0; // ���������� ���������
}