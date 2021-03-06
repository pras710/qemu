/* Copyright (C) 2007-2008 The Android Open Source Project
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*/
#include "gemdroid-tracer.h"
#include "migration/qemu-file.h"
#include "sysemu/char.h"
#include "hw/android/goldfish/device.h"
#include "hw/android/goldfish/vmem.h"
#include "hw/hw.h"

enum {
    TTY_PUT_CHAR       = 0x00,
    TTY_BYTES_READY    = 0x04,
    TTY_CMD            = 0x08,

    TTY_DATA_PTR       = 0x10,
    TTY_DATA_LEN       = 0x14,
    TTY_DATA_PTR_HIGH  = 0x18,

    TTY_CMD_INT_DISABLE    = 0,
    TTY_CMD_INT_ENABLE     = 1,
    TTY_CMD_WRITE_BUFFER   = 2,
    TTY_CMD_READ_BUFFER    = 3,
};

struct tty_state {
    struct goldfish_device dev;
    CharDriverState *cs;
    uint64_t ptr;
    uint32_t ptr_len;
    uint32_t ready;
    uint8_t data[128];
    uint32_t data_count;
};

#define  GOLDFISH_TTY_SAVE_VERSION  2

static void  goldfish_tty_save(QEMUFile*  f, void*  opaque)
{
    struct tty_state*  s = opaque;

    qemu_put_be64( f, s->ptr );
    qemu_put_be32( f, s->ptr_len );
    qemu_put_byte( f, s->ready );
    qemu_put_byte( f, s->data_count );
    qemu_put_buffer( f, s->data, s->data_count );
}

static int  goldfish_tty_load(QEMUFile*  f, void*  opaque, int  version_id)
{
    struct tty_state*  s = opaque;

    if ((version_id != GOLDFISH_TTY_SAVE_VERSION) && 
        (version_id != (GOLDFISH_TTY_SAVE_VERSION - 1))) {
        return -1;
    }
    if (version_id == (GOLDFISH_TTY_SAVE_VERSION - 1)) {
        s->ptr    = (uint64_t)qemu_get_be32(f);
    } else {
        s->ptr    = qemu_get_be64(f);
    }
    s->ptr_len    = qemu_get_be32(f);
    s->ready      = qemu_get_byte(f);
    s->data_count = qemu_get_byte(f);
    qemu_get_buffer(f, s->data, s->data_count);

    return 0;
}

static uint32_t goldfish_tty_read(void *opaque, hwaddr offset)
{
    struct tty_state *s = (struct tty_state *)opaque;

    //printf("goldfish_tty_read %x %x\n", offset, size);

    switch (offset) {
        case TTY_BYTES_READY:
            return s->data_count;
    default:
        cpu_abort(cpu_single_env,
                  "goldfish_tty_read: Bad offset %" HWADDR_PRIx "\n",
                  offset);
        return 0;
    }
}
static int printLength = 300;
static char pras_temp_line[1000];
static int pras_line_length = 0;
static void goldfish_tty_write(void *opaque, hwaddr offset, uint32_t value)
{
    struct tty_state *s = (struct tty_state *)opaque;

    //printf("goldfish_tty_write %x %x %x\n", offset, value, size);

	//if(s->cs && cmdNotIssued)
	//{
	//	qemu_chr_write(s->cs,"logcat -v thread",16);
	//	cmdNotIssued--;
	//}
    switch(offset) {
        case TTY_PUT_CHAR: {
            uint8_t ch = value;
            if(s->cs)
			{
				qemu_chr_write(s->cs, &ch, 1);
			}
        } break;

        case TTY_CMD:
            switch(value) {
                case TTY_CMD_INT_DISABLE:
                    if(s->ready) {
                        if(s->data_count > 0)
                            goldfish_device_set_irq(&s->dev, 0, 0);
                        s->ready = 0;
                    }
                    break;

                case TTY_CMD_INT_ENABLE:
                    if(!s->ready) {
                        if(s->data_count > 0)
                            goldfish_device_set_irq(&s->dev, 0, 1);
                        s->ready = 1;
                    }
                    break;

                case TTY_CMD_WRITE_BUFFER:
                    if(s->cs) {
                        int len;
                        target_ulong  buf;

                        buf = s->ptr;
                        len = s->ptr_len;

                        while (len) {
                            char   temp[64];
                            int    to_write = sizeof(temp);
                            if (to_write > len)
                                to_write = len;

                            safe_memory_rw_debug(current_cpu, buf, (uint8_t*)temp, to_write, 0);
							//if(needed_pid == -1)
							{
								int my_iter = 0;
								for(my_iter = 0; my_iter < to_write; my_iter++)
								{
									pras_temp_line[pras_line_length] = temp[my_iter];
									pras_temp_line[pras_line_length+1] = '\0';
									if(temp[my_iter] == '\n')
									{
										//if(strstr(pras_temp_line, "ActivityManager"))
										if(strstr(pras_temp_line, needed_proc_name))
										{
											//pras
											//printf("activitymanager: %s\n",pras_temp_line);
											if(pras_temp_line[1] == ' ')
											{
												int temp_it = 0, tid_now = 0;
												for(temp_it = 0; temp_it < 9; temp_it++)
												{
													//printf("looking at %c ", pras_temp_line[6+temp_it]);
													if(pras_temp_line[6+temp_it] != ' ')
													{
														tid_now = (tid_now*10)+(pras_temp_line[6+temp_it] - '0');
														//printf("tid = %d\n", tid_now);
													}
													if(tid_now != 0 && pras_temp_line[6+temp_it]==' ')break;
												}
												//printf("need_id = %d\n", tid_now);

												needed_pid = tid_now;
												if(needed_pid == 0)
												{
													printf("GUNDABAD: ttying wrong? %s ****%s*****\n", pras_temp_line, needed_proc_name);
													exit(0);
												}
												int tIter = 0;
												for(tIter = 0; tIter < needed_tid_length; tIter++)
												{
													if(tid_now == needed_tids[tIter])
													{
														//int temp = needed_tids[0];
														//needed_tids[0]=tid_now;
														//needed_tids[tIter] = temp;
														//temp = print_tids[0];
														//print_tids[0]=print_tids[tIter];
														//print_tids[tIter] = temp;
														//printf("inserting new tid: %d [%s] as %dth place <><> needed_tids[0]=%d, needed_tids[len]=%d\n", tid_now, pras_temp_line, tIter, needed_tids[0], needed_tids[tIter]);
														break;
													}
												}
												if(tIter == needed_tid_length)
												{
													if(needed_tid_length > 0)
													{
														int temp = needed_tids[0];
														needed_tids[0]=tid_now;
														needed_tids[tIter] = temp;
														temp = print_tids[0];
														print_tids[0]= 0;//print_tids[tIter];
														print_tids[tIter] = temp;
														printf("inserting new tid: %d [%s] as %dth place <><> needed_tids[0]=%d, needed_tids[len]=%d\n", tid_now, pras_temp_line, tIter, needed_tids[0], needed_tids[tIter]);
														if(needed_tid_length == 999)
														{
															printf("tracking 999 threads!!!\n");
															exit(0);
														}
													}
													else
													{
														needed_tids[tIter] = tid_now;
														printf("inserting new tid: %d [%s] as %dth place <><> needed_tids[0]=%d, needed_tids[len]=%d\n", tid_now, pras_temp_line, tIter, needed_tids[0], needed_tids[tIter]);
													}
													needed_tid_length++;
												}
											}
										}
										//pras_temp_line[0]='\0';
										pras_line_length = 0;
									}
#if 0									
									//I/ActivityManager(  277): Start proc com.android.calculator2 for activity com.android.calculator2/.Calculator: pid=712 uid=10011 gids={50011, 1028}
									if(temp[my_iter]=='\n' /*&& strstr(pras_temp_line, "ActivityManager") */&& strstr(pras_temp_line, "Start proc") && strstr(pras_temp_line, needed_proc_name))
									{
										printf("***********\n%s\n***************\n found proc =  %s\n", pras_temp_line, needed_proc_name);
										//Start proc com.android.calculator2 for activity com.android.calculator2/.Calculator: pid=698 uid=10011 gids=
										char *pid_string_t = (strstr(pras_temp_line, "pid")+4);
										printf("**\n%s\n**\n", pid_string_t);
										if(*(pid_string_t+3)==' ')
										{
											char pid_s[10] = {'(',' ',' ',*pid_string_t, *(pid_string_t+1), *(pid_string_t+2),':',' ', '\0', '\0'};
											needed_pid = (*(pid_string_t + 0) - '0') * 100 + (*(pid_string_t + 1) - '0') * 10 +(*(pid_string_t + 2) - '0') * 1 ;
											int iTemp = 0;
											for(iTemp = 0; iTemp < 10; iTemp++)
											{
												pid_string[iTemp] = pid_s[iTemp];
											}
											printf("line: %s \n%s\n[found pid? = %d]*%s*\n",pras_temp_line, pid_s, needed_pid, pid_string);
										}
										else
										{
											char pid_s[10] = {'(',' ',' ',*pid_string_t, *(pid_string_t+1), *(pid_string_t+2),*(pid_string_t+3),':',' ', '\0'};
											needed_pid = (*(pid_string_t + 0) - '0') * 1000 + (*(pid_string_t + 1) - '0') * 100 +(*(pid_string_t + 2) - '0') * 10 + (*(pid_string_t + 3) - '0') * 1 ;
											int iTemp = 0;
											for(iTemp = 0; iTemp < 10; iTemp++)
											{
												pid_string[iTemp] = pid_s[iTemp];
											}
											printf("line: %s \n%s\n[found pid? = %d]*%s*\n",pras_temp_line, pid_s, needed_pid, pid_string);
										}
									//	needed_pid >>= 8;
										
									}
#endif
									pras_line_length++;
								}
							}
#if 0
							else
							{
								//printf("%d %s\n", needed_pid, pid_string);
								//found a pid. track all threads in that pid now..
								int my_iter = 0;
								for(my_iter = 0; my_iter < to_write; my_iter++)
								{
									pras_temp_line[pras_line_length] = temp[my_iter];
									pras_temp_line[pras_line_length+1] = '\0';
									if(temp[my_iter] == '\n')
									{
										int my_tid_iter = 0; 
										for(my_tid_iter = 0; my_tid_iter < needed_tid_length; my_tid_iter ++)
										{
											char tid_string[50] = "from pid ";
											sprintf(tid_string, "%d", needed_tids[my_tid_iter]);
											//if(strstr(pras_temp_line, "ActivityManager"))
											if(strstr(pras_temp_line, "START ") && strstr(pras_temp_line, tid_string))
											{
												printf("tid started a new tid..%s\n", pras_temp_line);
												int tid_now = -1;
												if(pras_temp_line[10] == ' ')
												{
													tid_now = (pras_temp_line[11]-'0') * 100+
														(pras_temp_line[12]-'0') * 10+
														(pras_temp_line[13]-'0') * 1;
												}
												else
												{
													tid_now = (pras_temp_line[10]-'0') * 1000+
														(pras_temp_line[11]-'0') * 100+
														(pras_temp_line[12]-'0') * 10+
														(pras_temp_line[13]-'0') * 1;
												}
												printf("tid_now = %d\n",tid_now);
												int tIter = 0;
												for(tIter = 0; tIter < needed_tid_length; tIter++)
												{
													if(tid_now == needed_tids[tIter])
													{
														break;
													}
												}
												if(tIter == needed_tid_length)
												{
													needed_tids[tIter] = tid_now;
													if(needed_tid_length == 99)
													{
														printf("tracking 99 threads!!!\n");
														exit(0);
													}
													needed_tid_length++;
													printf("tids tracked = %d\n", needed_tid_length);
												}
											}
										}
										if(strstr(pras_temp_line, pid_string))
										{
											//pras
											int tid_now =-1;
											if(pras_temp_line[10] == ' ')
											{
												printf("%s: activitymanager: %s\n tid chars = %c %c %c\n",pid_string, pras_temp_line, pras_temp_line[11], pras_temp_line[12], pras_temp_line[13]);
												tid_now = (pras_temp_line[11]-'0') * 100+
															(pras_temp_line[12]-'0') * 10+
															(pras_temp_line[13]-'0') * 1;
											}
											else
											{
												printf("%s: activitymanager: %s\n tid chars = %c %c %c %c\n",pid_string, pras_temp_line, pras_temp_line[10], pras_temp_line[11], pras_temp_line[12], pras_temp_line[13]);
												tid_now = (pras_temp_line[10]-'0') * 1000+
															(pras_temp_line[11]-'0') * 100+
															(pras_temp_line[12]-'0') * 10+
															(pras_temp_line[13]-'0') * 1;
											}
											printf("tid_now = %d\n",tid_now);
											int tIter = 0;
											for(tIter = 0; tIter < needed_tid_length; tIter++)
											{
												if(tid_now == needed_tids[tIter])
												{
													break;
												}
											}
											if(tIter == needed_tid_length)
											{
												needed_tids[tIter] = tid_now;
												if(needed_tid_length == 99)
												{
													printf("tracking 99 threads!!!\n");
													exit(0);
												}
												needed_tid_length++;
											}
										}
										//pras_temp_line[0]='\0';
										pras_line_length = 0;
									}
									pras_line_length++;
								}
							}
#endif
							//temp[strlen(temp)]='\0';
							//printf("%s", temp);
							//if(strstr(pras_temp, "pid="))
							//{
							//	printf("line = %s\n", pras_temp);
							//}
                            //PRAS: this is not commented initially
							//qemu_chr_write(s->cs, (const uint8_t*)temp, to_write);
							if(needed_pid == -1 && printLength)
							{
								qemu_chr_write(s->cs, (const uint8_t*)temp, to_write);
								printLength--;
							}
							//PRAS: end
                            buf += to_write;
                            len -= to_write;
                        }
						

						//printf("pras identifies: %s\n",pras_temp); 
                        //printf("goldfish_tty_write: got %d bytes from %llx \n", s->ptr_len, (unsigned long long)s->ptr);
                    }
                    break;

                case TTY_CMD_READ_BUFFER:
                    if(s->ptr_len > s->data_count)
                        cpu_abort (cpu_single_env, "goldfish_tty_write: reading more data than available %d %d\n", s->ptr_len, s->data_count);
                    safe_memory_rw_debug(current_cpu, s->ptr, s->data, s->ptr_len,1);
                    //printf("goldfish_tty_write: read %d bytes to %llx\n", s->ptr_len, (unsigned long long)s->ptr);
                    if(s->data_count > s->ptr_len)
                        memmove(s->data, s->data + s->ptr_len, s->data_count - s->ptr_len);
                    s->data_count -= s->ptr_len;
                    if(s->data_count == 0 && s->ready)
                        goldfish_device_set_irq(&s->dev, 0, 0);
                    break;

                default:
                    cpu_abort (cpu_single_env, "goldfish_tty_write: Bad command %x\n", value);
            };
            break;

        case TTY_DATA_PTR:
            uint64_set_low(&s->ptr, value);
            break;

        case TTY_DATA_PTR_HIGH:
            uint64_set_high(&s->ptr, value);
            break;

        case TTY_DATA_LEN:
            s->ptr_len = value;
            break;

        default:
            cpu_abort(cpu_single_env,
                      "goldfish_tty_write: Bad offset %" HWADDR_PRIx "\n",
                      offset);
    }
}

static int tty_can_receive(void *opaque)
{
    struct tty_state *s = opaque;

    return (sizeof(s->data) - s->data_count);
}

static void tty_receive(void *opaque, const uint8_t *buf, int size)
{
    struct tty_state *s = opaque;

    memcpy(s->data + s->data_count, buf, size);
    s->data_count += size;
    if(s->data_count > 0 && s->ready)
        goldfish_device_set_irq(&s->dev, 0, 1);
}

static CPUReadMemoryFunc *goldfish_tty_readfn[] = {
    goldfish_tty_read,
    goldfish_tty_read,
    goldfish_tty_read
};

static CPUWriteMemoryFunc *goldfish_tty_writefn[] = {
    goldfish_tty_write,
    goldfish_tty_write,
    goldfish_tty_write
};

int goldfish_tty_add(CharDriverState *cs, int id, uint32_t base, int irq)
{
    int ret;
    struct tty_state *s;
    static int  instance_id = 0;

    s = g_malloc0(sizeof(*s));
    s->dev.name = "goldfish_tty";
    s->dev.id = id;
    s->dev.base = base;
    s->dev.size = 0x1000;
    s->dev.irq = irq;
    s->dev.irq_count = 1;
    s->cs = cs;

    if(cs) {
        qemu_chr_add_handlers(cs, tty_can_receive, tty_receive, NULL, s);
    }

    ret = goldfish_device_add(&s->dev, goldfish_tty_readfn, goldfish_tty_writefn, s);
    if(ret) {
        g_free(s);
    } else {
        register_savevm(NULL,
                        "goldfish_tty",
                        instance_id++,
                        GOLDFISH_TTY_SAVE_VERSION,
                        goldfish_tty_save,
                        goldfish_tty_load,
                        s);
    }
    return ret;
}

