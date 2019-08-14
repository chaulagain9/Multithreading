/*
Name: Sameer Chaulagain
ID:   1001418268
*/

#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

/*** Constants that define parameters of the simulation ***/

#define MAX_SEATS 3        /* Number of seats in the professor's office */
#define professor_LIMIT 10 /* Number of students the professor can help before he needs a break */
#define MAX_STUDENTS 1000  /* Maximum number of students in the simulation */

#define CLASSA 0
#define CLASSB 1
#define CLASSC 2
#define CLASSD 3

/* Mutex will allow access only from one thread at the time for all global variables */
pthread_mutex_t mut;

pthread_cond_t cond_enter_a; /* Conditional variable to allow students of class A to enter */
pthread_cond_t cond_enter_b; /* Conditional variable to allow students of class B to enter */

static int classa_consecutive; /* Total numbers of consecutive students from class A */
static int classb_consecutive; /* Total numbers of consecutive students from class B */
static int classa_waiting;     /* Total numbers of students from class A that are waitnig outside the office */
static int classb_waiting;     /* Total numbers of students from class B that are waitnig outside the office */
static int classa_may_enter;   /* If student from class A may enter */
static int classb_may_enter;   /* If student from class B may enter */

/* This variable will show is it possible to enter the office for the student from class B */
static int classb_permission;
/* Basic information about simulation.  They are printed/checked at the end 
 * and in assert statements during execution.
 *
 * You are responsible for maintaining the integrity of these variables in the 
 * code that you develop. 
 */

static int students_in_office; /* Total numbers of students currently in the office */
static int classa_inoffice;    /* Total numbers of students from class A currently in the office */
static int classb_inoffice;    /* Total numbers of students from class B in the office */
static int students_since_break = 0;

typedef struct
{
  int arrival_time;  // time between the arrival of this student and the previous student
  int question_time; // time the student needs to spend with the professor
  int student_id;
  int class;
} student_info;

/* Called at beginning of simulation.  
 * TODO: Create/initialize all synchronization
 * variables and other global variables that you add.
 */
static int initialize(student_info *si, char *filename)
{
  students_in_office = 0;
  classa_inoffice = 0;
  classb_inoffice = 0;
  students_since_break = 0;

  /* Initialize your synchronization variables */
  pthread_mutex_init(&mut, NULL);
  pthread_cond_init(&cond_enter_a, NULL);
  pthread_cond_init(&cond_enter_b, NULL);
  classa_consecutive = 0;
  classb_consecutive = 0;
  classa_waiting = 0;
  classb_waiting = 0;
  classa_may_enter = 0;
  classb_may_enter = 0;

  /* Read in the data file and initialize the student array */
  FILE *fp;

  if ((fp = fopen(filename, "r")) == NULL)
  {
    printf("Cannot open input file %s for reading.\n", filename);
    exit(1);
  }

  int i = 0;
  while ((fscanf(fp, "%d%d%d\n", &(si[i].class), &(si[i].arrival_time), &(si[i].question_time)) != EOF) &&
         i < MAX_STUDENTS)
  {
    i++;
  }

  fclose(fp);
  return i;
}

/* Code executed by professor to simulate taking a break 
 * You do not need to add anything here.  
 */
static void take_break()
{
  printf("The professor is taking a break now.\n");
  sleep(5);
  assert(students_in_office == 0);
  students_since_break = 0;
}

/* Code for the professor thread. This is fully implemented except for synchronization
 * with the students.  See the comments within the function for details.
 */
void *professorthread(void *junk)
{
  printf("The professor arrived and is starting his office hours\n");

  /* Loop while waiting for students to arrive. */
  while (1)
  {
    /* lock all common counters */
    pthread_mutex_lock(&mut);

    /* Professor gets tired after answering too many questions (professor_LIMIT) */
    if (students_since_break == professor_LIMIT && students_in_office == 0)
    {
      take_break();
    }
    /* Professor gives permission for students to enter */
    /* Student from class A may enter the office if:
       1. There is available seat in the office
       2. No students from class B at the office 
       (The professor gets confused when helping students from class A and class B at the same time. )
       3. Professor is not tired (helped no more than professor_LIMIT students after last break) 
       4. No more than 5 consecutive students from class A entered the office 
          and there is waiting student from class B
    */
    classa_may_enter = students_in_office < MAX_SEATS && classb_inoffice == 0 &&
                       students_since_break < professor_LIMIT && (classa_consecutive < 5 || classb_waiting == 0);

    /* Student from class B may enter the office if:
       1. There is available seat in the office
       2. No students from class A at the office 
       (The professor gets confused when helping students from class A and class B at the same time. )
       3. Professor is not tired (helped no more than professor_LIMIT students after last break) 
       4. No more than 5 consecutive students from class B entered the office 
          and there is waiting student from class A
    */
    classb_may_enter = students_in_office < MAX_SEATS && classa_inoffice == 0 &&
                       students_since_break < professor_LIMIT && (classb_consecutive < 5 || classa_waiting == 0);

    if (classa_may_enter && classa_waiting > 0)
    {
      pthread_cond_broadcast(&cond_enter_a); /* Awake sleeping threads from class A */
    }
    else if (classb_may_enter && classb_waiting > 0)
    {
      pthread_cond_broadcast(&cond_enter_b); /* Awake sleeping threads from class B */
    }

    /* Unlock all common counters */
    pthread_mutex_unlock(&mut);
  }

  pthread_exit(NULL);
}

/* Code executed by a class A student to enter the office.
 * You have to implement this.  Do not delete the assert() statements,
 * but feel free to add your own.
 */
void classa_enter()
{

  /* Request permission to enter the office. */

  /* lock all common counters */
  pthread_mutex_lock(&mut);
  classa_waiting += 1;

  while (!classa_may_enter)
  {
    pthread_cond_wait(&cond_enter_a, &mut); /* wait until cond_enter_a resumes */
  }
  students_in_office += 1;
  students_since_break += 1;
  classa_inoffice += 1;
  classa_waiting -= 1; /* student already not waiting */
  classa_consecutive += 1;
  classb_consecutive = 0;
  /* one student entered, reset permision flags for all classes */
  classa_may_enter = 0;
  classb_may_enter = 0;

  /* Unlock all common counters */
  pthread_mutex_unlock(&mut);
}

/* Code executed by a class B student to enter the office.
 * You have to implement this.  Do not delete the assert() statements,
 * but feel free to add your own.
 */
void classb_enter()
{

  /* Request permission to enter the office.  */
  /* lock all common counters */
  pthread_mutex_lock(&mut);
  classb_waiting += 1;

  while (!classb_may_enter)
  {
    pthread_cond_wait(&cond_enter_b, &mut); /* wait until cond_enter_b resumes */
  }
  students_in_office += 1;
  students_since_break += 1;
  classb_inoffice += 1;
  classb_waiting -= 1; /* student already not waiting */
  classb_consecutive += 1;
  classa_consecutive = 0;
  /* one student entered, reset permision flags for all classes */
  classa_may_enter = 0;
  classb_may_enter = 0;

  /* Unlock all common counters */
  pthread_mutex_unlock(&mut);
}

/* Code executed by a student to simulate the time he spends in the office asking questions
 * You do not need to add anything here.  
 */
static void ask_questions(int t)
{
  sleep(t);
}

/* Code executed by a class A student when leaving the office.
 * You need to implement this.  Do not delete the assert() statements,
 * but feel free to add as many of your own as you like.
 */
static void classa_leave()
{
  pthread_mutex_lock(&mut);
  students_in_office -= 1;
  classa_inoffice -= 1;
  pthread_mutex_unlock(&mut);
}

/* Code executed by a class B student when leaving the office.
 * You need to implement this.  Do not delete the assert() statements,
 * but feel free to add as many of your own as you like.
 */
static void classb_leave()
{
  pthread_mutex_lock(&mut);
  students_in_office -= 1;
  classb_inoffice -= 1;
  pthread_mutex_unlock(&mut);
}

/* Main code for class A student threads.  
 * You do not need to change anything here, but you can add
 * debug statements to help you during development/debugging.
 */
void *classa_student(void *si)
{
  student_info *s_info = (student_info *)si;

  /* enter office */
  classa_enter();

  printf("Student %d from class A enters the office\n", s_info->student_id);

  assert(students_in_office <= MAX_SEATS && students_in_office >= 0);
  assert(classa_inoffice >= 0 && classa_inoffice <= MAX_SEATS);
  assert(classb_inoffice >= 0 && classb_inoffice <= MAX_SEATS);
  assert(classb_inoffice == 0);

  /* ask questions  --- do not make changes to the 3 lines below*/
  printf("Student %d from class A starts asking questions for %d minutes\n", s_info->student_id, s_info->question_time);
  ask_questions(s_info->question_time);
  printf("Student %d from class A finishes asking questions and prepares to leave\n", s_info->student_id);

  /* leave office */
  classa_leave();

  printf("Student %d from class A leaves the office\n", s_info->student_id);

  assert(students_in_office <= MAX_SEATS && students_in_office >= 0);
  assert(classb_inoffice >= 0 && classb_inoffice <= MAX_SEATS);
  assert(classa_inoffice >= 0 && classa_inoffice <= MAX_SEATS);

  pthread_exit(NULL);
}

/* Main code for class B student threads.
 * You do not need to change anything here, but you can add
 * debug statements to help you during development/debugging.
 */
void *classb_student(void *si)
{
  student_info *s_info = (student_info *)si;

  /* enter office */
  classb_enter();

  printf("Student %d from class B enters the office\n", s_info->student_id);

  assert(students_in_office <= MAX_SEATS && students_in_office >= 0);
  assert(classb_inoffice >= 0 && classb_inoffice <= MAX_SEATS);
  assert(classa_inoffice >= 0 && classa_inoffice <= MAX_SEATS);
  assert(classa_inoffice == 0);

  printf("Student %d from class B starts asking questions for %d minutes\n", s_info->student_id, s_info->question_time);
  ask_questions(s_info->question_time);
  printf("Student %d from class B finishes asking questions and prepares to leave\n", s_info->student_id);

  /* leave office */
  classb_leave();

  printf("Student %d from class B leaves the office\n", s_info->student_id);

  assert(students_in_office <= MAX_SEATS && students_in_office >= 0);
  assert(classb_inoffice >= 0 && classb_inoffice <= MAX_SEATS);
  assert(classa_inoffice >= 0 && classa_inoffice <= MAX_SEATS);

  pthread_exit(NULL);
}

/* Main function sets up simulation and prints report
 * at the end.
 */
int main(int nargs, char **args)
{
  int i;
  int result;
  int student_type;
  int num_students;
  void *status;
  pthread_t professor_tid;
  pthread_t student_tid[MAX_STUDENTS];
  student_info s_info[MAX_STUDENTS];

  if (nargs != 2)
  {
    printf("Usage: officehour <name of inputfile>\n");
    return EINVAL;
  }

  num_students = initialize(s_info, args[1]);
  if (num_students > MAX_STUDENTS || num_students <= 0)
  {
    printf("Error:  Bad number of student threads. "
           "Maybe there was a problem with your input file?\n");
    return 1;
  }

  printf("Starting officehour simulation with %d students ...\n",
         num_students);

  result = pthread_create(&professor_tid, NULL, professorthread, NULL);

  if (result)
  {
    printf("officehour:  pthread_create failed for professor: %s\n", strerror(result));
    exit(1);
  }

  for (i = 0; i < num_students; i++)
  {

    s_info[i].student_id = i;
    sleep(s_info[i].arrival_time);

    student_type = random() % 2;

    if (s_info[i].class == CLASSA)
    {
      result = pthread_create(&student_tid[i], NULL, classa_student, (void *)&s_info[i]);
    }
    else // student_type == CLASSB
    {
      result = pthread_create(&student_tid[i], NULL, classb_student, (void *)&s_info[i]);
    }

    if (result)
    {
      printf("officehour: thread_fork failed for student %d: %s\n",i, strerror(result));
      exit(1);
    }
  }

  /* wait for all student threads to finish */
  for (i = 0; i < num_students; i++)
  {
    pthread_join(student_tid[i], &status);
  }

  /* tell the professor to finish. */
  pthread_cancel(professor_tid);

  printf("Office hour simulation done.\n");

  return 0;
}
