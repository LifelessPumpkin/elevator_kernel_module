# Elevator

A simple elevator kernel module that accepts requests from pets.

## Group Members
- **Logan Harmon**: lmh22c@fsu.edu
## Division of Labor

### Part 1: System Call Tracing
- **Assigned to**: Logan Harmon

### Part 2: Timer Kernel Module
- **Assigned to**: Logan Harmon

### Part 3a: Adding System Calls
- **Assigned to**: Logan Harmon

### Part 3b: Kernel Compilation
- **Assigned to**: Logan Harmon

### Part 3c: Threads
- **Assigned to**: Logan Harmon

### Part 3d: Linked List
- **Assigned to**: Logan Harmon

### Part 3e: Mutexes
- **Assigned to**: Logan Harmon

### Part 3f: Scheduling Algorithm
- **Assigned to**: Logan Harmon

## File Listing
```
elevator/
├── Makefile
├── part1/
│   ├── empty.c
│   ├── empty.trace
│   ├── part1.c
│   ├── part1.trace
│   └── Makefile
├── part2/
│   ├── my_timer.c
│   └── Makefile
├── part3/
│   ├── src/
│   └── syscalls.c
├── Makefile
└── README.md

```
# How to Compile & Execute

### Requirements
- **Compiler**: gcc

## Part 1

### Compilation
```bash
make
```
### Execution
```bash
./empty
./part1
```

## Part 2

### Compilation
```bash
make
```
### Execution
```bash
sudo insmod my_timer.ko
cat /proc/timer
sudo rmmod my_timer.ko
```

## Part 3

### Compilation
```bash
make
```
### Installation
```bash
sudo insmod elevator.ko
```

### Running the program
```bash
./consumer --start
./producer (any number of test cases)
.consumer --stop
```

### Remove installation
```bash
sudo rmmod elevtor
```

## Development Log

### [Logan Harmon]

| Date       | Work Completed / Notes |
|------------|------------------------|
| 2025-10-12 | Finished part1  |
| 2025-10-13 | Finished part 2  |
| 2025-10-24 | Compiled and installed kernel  |
| 2025-10-30 | Implemented the syscall functions  |
| 2025-10-31 | Made a bunch of the main logic |
| 2025-11-02 | Finished up the project  |

## Meetings

- None, it was just me.

## Bugs
- I shouldn't have any bugs

## Considerations
- None