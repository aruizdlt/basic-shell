/**
 UNIX Shell Project

 Sistemas Operativos
 Grados I. Informatica, Computadores & Software
 Dept. Arquitectura de Computadores - UMA

 Some code adapted from "Fundamentos de Sistemas Operativos", Silberschatz et al.

 To compile and run the program:
 $ gcc Shell_project.c job_control.c -o Shell
 $ ./Shell
	(then type ^D to exit program)

 **/

#include "job_control.h"   // remember to compile with module job_control.c
#include <string.h>

#define MAX_LINE 256 /* 256 chars per line, per command, should be enough. */

// -----------------------------------------------------------------------
//                            MAIN
// -----------------------------------------------------------------------

job* list;

void manejador(){
  int status;
  int info;
  enum status status_res;
  int pid_wait;

  int boolean = 0;

  job * aux;
  job * listaAux = list;

  block_SIGCHLD();
  if(empty_list(listaAux)){
      printf("Lista de procesos vacía \n");
  }else{
      listaAux = listaAux->next;
      while(listaAux != NULL){
          pid_wait = waitpid(listaAux->pgid, &status, WNOHANG | WUNTRACED);
          if(pid_wait == (listaAux->pgid)){
              status_res = analyze_status(status, &info);
              if(status_res == SUSPENDED){
                  listaAux->state = STOPPED;
                  listaAux = listaAux->next;
              }else if(status_res == SIGNALED || status_res == EXITED){
                  aux = listaAux;
                  printf("\nBorrando pid: %d, Command: %s, %s, Info: %d\n",aux->pgid,aux->command,status_strings[status_res],info);
                  listaAux = listaAux->next;
                  boolean = 1;
                  delete_job(list,aux);
                  free_job(aux);
              }else{
                  listaAux = listaAux->next;
              }
          }else{
            listaAux = listaAux->next;
          }
        }
      }
      unblock_SIGCHLD();
}


int main(void)
{
    char inputBuffer[MAX_LINE]; /* buffer to hold the command entered */
    int background;             /* equals 1 if a command is followed by '&' */
    char *args[MAX_LINE/2];     /* command line (of 256) has max of 128 arguments */
    // probably useful variables:
    int pid_fork, pid_wait; /* pid for created and waited process */
    int status;             /* status returned by wait */
    enum status status_res; /* status processed by analyze_status() */
    int info;				/* info processed by analyze_status() */

    list = new_list("ListaProc");
    signal(SIGCHLD,manejador);

    while (1)   /* Program terminates normally inside get_command() after ^D is typed*/
    {
        ignore_terminal_signals();

        printf("COMMAND->");
        fflush(stdout);
        get_command(inputBuffer, MAX_LINE, args, &background);  /* get next command */

        if(args[0]==NULL){
           continue; // if empty command
        }else if(strcmp(args[0],"cd")== 0){
            if(-1 == chdir(args[1]))
              perror(args[1]);
            continue;

        }else if(strcmp(args[0],"jobs")==0){
          if(empty_list(list)){
            printf("La lista de procesos está vacía\n");
          }else{
            block_SIGCHLD();
            print_job_list(list);
            unblock_SIGCHLD();
          }
          continue;
        }else if(strcmp(args[0],"fg") == 0){
          if(empty_list(list)){
            printf("The list is empty!\n");
          }else{
            int num = 1;
            if(args[1] != NULL){
              sscanf(args[1],"%d",&num);
            }
            if(num <= 0 || num > list_size(list)){
              printf("No existe esa posición en la lista.");
            }else{
              job * aux = get_item_bypos(list,num);
              aux->state = FOREGROUND;
              set_terminal(aux->pgid);
              killpg(aux->pgid, SIGCONT);
              pid_wait = waitpid(aux->pgid, &status, WUNTRACED); //esperamos a la señal de terminación.
              status_res = analyze_status(status, &info); //analizamos como ha terminado el proceso
              printf("Foreground pid %d, command: %s, %s, info: %d\n", aux->pgid, aux->command, status_strings[status_res], info);
              if(status_res == SUSPENDED){
                aux->state = STOPPED;
              }
              if(status_res == SIGNALED || status_res == EXITED){
                printf("\nBorrando pid: %d, Command: %s, %s, Info: %d\n",aux->pgid,aux->command,status_strings[status_res],info);
                delete_job(list,aux);
                free_job(aux);
              }
              set_terminal(getpid());
              }
            }
          continue;
        }else if(strcmp(args[0],"bg") == 0){
          if(empty_list(list)){
            printf("The list is empty!\n");
          }else{
            int num = 1;
            if(args[1] != NULL){
              sscanf(args[1],"%d",&num);
            }
            if(num <= 0 || num > list_size(list)){
              printf("No existe esa posición en la lista.");
            }else{
              job * aux = get_item_bypos(list,num);
              aux->state = BACKGROUND;
              killpg(aux->pgid,SIGCONT);
              printf("Background job running... pid: %d, command: %s\n", aux->pgid,aux->command);
              if(aux->state == SUSPENDED){
                aux->state = STOPPED;
              }
            }
          }
          continue;
        }else{
            pid_fork = fork();
            if(pid_fork == 0){//proceso hijo recien creado
                restore_terminal_signals(); //restaura las señales de terminal: comandos que se pueden usar.
                pid_fork = getpid(); //pid_fork pierde el 0 y se le asocia el pid real del proceso creado.
                new_process_group(pid_fork); // crea un grupo de procesos independientes.
                if(background == 0){
                    set_terminal(pid_fork); //asignación del terminal si el proceso es fg.
                }
                execvp(args[0],args);
                perror("Error, command not found");
                exit(-1);
            }else{ //proceso padre.
                new_process_group(pid_fork);
                if(background == 0){
                    set_terminal(pid_fork);
                    pid_wait = waitpid(pid_fork, &status, WUNTRACED); //esperamos a la señal de terminación.
                    status_res = analyze_status(status, &info); //analizamos como ha terminado el proceso
                    if(status_res == SUSPENDED){
                      block_SIGCHLD();
                      job * item = new_job(pid_fork,args[0],STOPPED);
                      add_job(list,item);
                      unblock_SIGCHLD();
                    }
                    printf("Foreground pid %d, command: %s, %s, info: %d\n", pid_wait, args[0], status_strings[status_res], info);
                    set_terminal(getpid());
                }else{ //proceso en background
                    printf("Background job running... pid: %d, command: %s\n", pid_fork,args[0]);
                    job * item = new_job(pid_fork,args[0],BACKGROUND);
                    block_SIGCHLD();
                    add_job(list,item);
                    unblock_SIGCHLD();
                }

            }
        }
    }
}
