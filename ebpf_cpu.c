// gcc ebpf_cpu.c -lbpf -o cpu
// gcc ebpf_cpu.c -o cpu -lbpf -lelf -lz

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>

struct sample {
    __u64 run_time_ns;
    __u64 run_cnt;
    struct timespec timestamp;
};

int collect_sample(int fd, struct sample *s) {

    struct bpf_prog_info info = {};
    __u32 len = sizeof(info);
    
    if (bpf_obj_get_info_by_fd(fd, &info, &len)) return -1;

    s->run_time_ns = info.run_time_ns;
    s->run_cnt = info.run_cnt;

    clock_gettime(CLOCK_MONOTONIC, &s->timestamp);

    return 0;
}

int main(int argc, char **argv) {

    if (argc < 3) {
        fprintf(stderr, "Usage: sudo %s <program_name> <seconds>\nMake sure to: echo 1 > /proc/sys/kernel/bpf_stats_enabled\n", argv[0]);
        return 1;
    }

    int tempo = atoi(argv[2]);

    const char *target_name = argv[1];
    int prog_fd = -1;
    __u32 id = 0;

    while (bpf_prog_get_next_id(id, &id) == 0) {

        int fd = bpf_prog_get_fd_by_id(id);
        struct bpf_prog_info info = {};
        __u32 len = sizeof(info);

        if (fd >= 0 && bpf_obj_get_info_by_fd(fd, &info, &len) == 0) {
            if (strcmp(info.name, target_name) == 0) {
                prog_fd = fd;
                break;
            }
        }
        close(fd);
    }

    if (prog_fd < 0) {
        fprintf(stderr, "Erro: Program '%s' not found.\n", target_name);
        return 1;
    }

    FILE *log_file = fopen("ebpf_stats.txt", "w+");
    if (!log_file) {
        perror("Error to open the file txt");
        close(prog_fd);
        return 1;
    }

    fprintf(log_file, "\n--- Monitoring '%s' ---\n", target_name);
    printf("Monitoring '%s'. Writting in 'ebpf_stats.txt'...\n", target_name);

    struct sample s_old, s_new;
    for(int i = 0; i <= tempo; i++) {
        if (collect_sample(prog_fd, &s_old) != 0) break;
        sleep(1);
        if (collect_sample(prog_fd, &s_new) != 0) break;

        __u64 diff_runtime_ns = s_new.run_time_ns - s_old.run_time_ns;
        __u64 diff_count = s_new.run_cnt - s_old.run_cnt;
        double diff_wall_ns = (s_new.timestamp.tv_sec - s_old.timestamp.tv_sec) * 1e9 +
                              (s_new.timestamp.tv_nsec - s_old.timestamp.tv_nsec);

        double cpu_perc = (diff_runtime_ns / diff_wall_ns) * 100.0;

        time_t now = time(NULL);
        char *ts = ctime(&now);
        ts[strlen(ts) - 1] = '\0'; // Remove o \n do ctime

        fprintf(log_file, "[%d] CPU: %.4f | Execs/s: %llu | Avg: %llu ns\n", /*ts*/i, cpu_perc, diff_count, diff_count > 0 ? diff_runtime_ns / diff_count : 0);
        fflush(log_file);

        printf("\r[%s] CPU: %7.4f%% | Execs/s: %-8llu", ts, cpu_perc, diff_count);
        fflush(stdout);
    }

    fclose(log_file);
    close(prog_fd);
    return 0;
}

