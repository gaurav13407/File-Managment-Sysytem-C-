#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <pthread.h>
#include <jpeglib.h>
#include <png.h>

#define MAX_FILES 100
#define LOG_FILE "conversion_logs.txt"

// Structure for thread arguments
typedef struct {
    char inputFile[MAX_FILES];
    char outputFile[MAX_FILES];
    char format[10];
} ThreadArgs;

// Global variables for GUI elements (simplified for illustration)
GtkWidget *input_dir_entry;
GtkWidget *output_dir_entry;
GtkWidget *format_combo;
GtkWidget *log_text_view;
GtkWidget *progress_bar;
GtkWidget *window;

// Function declarations
void showMessage(const char *message);
void updateLog(const char *message);
void updateProgress(double fraction);
void convertFilesBatch(char *inputDir, char *outputDir, char *format);
void *convertFileThread(void *arg);
void viewLogs();
void writeLog(const char *message);

// File conversion function (modified for GUI)
void convertFilesBatch(char *inputDir, char *outputDir, char *format) {
    DIR *dir;
    struct dirent *entry;
    int fileCount = 0;
    pthread_t threads[MAX_FILES];
    ThreadArgs args[MAX_FILES];
    int threadCount = 0;

    dir = opendir(inputDir);
    if (!dir) {
        perror("Error opening input directory");
        writeLog("Error opening input directory for batch conversion.");
        showMessage("Error opening input directory. Check logs."); // Use GUI message
        return;
    }

    // First, count the number of files to convert
     while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) { // Only process regular files.
            if (strstr(entry->d_name, ".txt") || strstr(entry->d_name, ".jpg") || strstr(entry->d_name, ".png")) {
                fileCount++;
            }
        }
    }
    rewinddir(dir);

    if (fileCount == 0) {
        showMessage("No files found to convert.");
        closedir(dir);
        return;
    }

    char message[MAX_FILES];
    snprintf(message, sizeof(message), "Starting batch conversion of %d files to %s...", fileCount, format);
    updateLog(message); // Use GUI log
    writeLog("Starting batch conversion.");

    // Create threads for each file
    while ((entry = readdir(dir)) != NULL) {
       if (entry->d_type == DT_REG) {
            if (strstr(entry->d_name, ".txt") || strstr(entry->d_name, ".jpg") || strstr(entry->d_name, ".png")) {
                snprintf(args[threadCount].inputFile, MAX_FILES, "%s/%s", inputDir, entry->d_name);
                snprintf(args[threadCount].outputFile, MAX_FILES, "%s/%s.%s", outputDir, entry->d_name, format);
                strcpy(args[threadCount].format, format);
                if (pthread_create(&threads[threadCount], NULL, convertFileThread, (void *)&args[threadCount]) != 0) {
                    perror("Failed to create thread");
                    writeLog("Failed to create thread");
                    showMessage("Failed to create thread. Conversion aborted.");
                    //  Handle error:  Stop conversion?  Try again?
                }
                else {
                  threadCount++;
                }
            }
       }
    }
    closedir(dir);

    // Wait for all threads to complete
    for (int i = 0; i < threadCount; i++) {
        pthread_join(threads[i], NULL);
         updateProgress((double)(i + 1) / fileCount); // Update progress bar
    }

    updateLog("Batch conversion complete.");
    writeLog("Batch conversion completed.");
    showMessage("Conversion complete!");
}

// Thread function (same as before)
void *convertFileThread(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    FILE *in, *out;
    char buffer[MAX_FILES];

    printf("Thread started for %s\n", args->inputFile); // Keep console output for debugging

    in = fopen(args->inputFile, "r");
    if (!in) {
        fprintf(stderr, "Error: Cannot open input file %s\n", args->inputFile);
        writeLog("Error: Cannot open input file in thread.");
        pthread_exit(NULL);
    }

    out = fopen(args->outputFile, "w");
    if (!out) {
        fprintf(stderr, "Error: Cannot open output file %s\n", args->outputFile);
        writeLog("Error: Cannot open output file in thread.");
        fclose(in);
        pthread_exit(NULL);
    }

    if (strcmp(args->format, "txt") == 0) {
        // TXT to TXT (copy)
        while (fgets(buffer, MAX_FILES, in)) {
            fprintf(out, "%s", buffer);
        }
    } else if (strcmp(args->format, "csv") == 0) {
        // TXT to CSV
        while (fgets(buffer, MAX_FILES, in)) {
            for (int i = 0; buffer[i]; i++) {
                if (buffer[i] == ' ')
                    buffer[i] = ',';
            }
            fprintf(out, "%s", buffer);
        }
    } else if (strcmp(args->format, "jpg") == 0) {
        // rudimentary copy
         while (fgets(buffer, MAX_FILES, in)) {
            fprintf(out, "%s", buffer);
         }
    } else if (strcmp(args->format, "png") == 0) {
        // rudimentary copy
        while (fgets(buffer, MAX_FILES, in)) {
            fprintf(out, "%s", buffer);
         }
    }else {
        fprintf(stderr, "Error: Unsupported format %s\n", args->format);
        writeLog("Error: Unsupported format in thread.");
        fclose(in);
        fclose(out);
        pthread_exit(NULL);
    }

    fclose(in);
    fclose(out);
    printf("Thread finished for %s\n", args->inputFile);  // Keep console output for debugging.
    writeLog("File conversion thread completed.");
    pthread_exit(NULL);
}

// View logs (modified to display in GUI)
void viewLogs() {
    FILE *log = fopen(LOG_FILE, "r");
    char line[MAX_FILES];
    if (!log) {
        showMessage("No logs found.");
        return;
    }
    char logContent[65536] = "";  // Read up to 64KB.
    while (fgets(line, sizeof(line), log)) {
       strcat(logContent, line);
    }
    fclose(log);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(log_text_view)), logContent, -1);
}

// Write to logs.txt (same as before)
void writeLog(const char *message) {
    FILE *log = fopen(LOG_FILE, "a");
    if (log) {
        fprintf(log, "%s\n", message);
        fclose(log);
    } else {
        perror("Error writing to log file");
    }
}

// Helper functions for GUI interaction
void showMessage(const char *message) {
    GtkWidget *dialog;
    dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_INFO,
                                     GTK_BUTTONS_OK,
                                     "%s", message);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void updateLog(const char *message) {
   GtkTextBuffer *buffer;
   GtkTextIter end;
   buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(log_text_view));
   gtk_text_buffer_get_end_iter(buffer, &end);
   gtk_text_buffer_insert(buffer, &end, message, -1);
   gtk_text_buffer_insert(buffer, &end, "\n", -1);
   // Make the log scroll to the bottom.
   GtkAdjustment *adjustment = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE(log_text_view));
   gtk_adjustment_set_value (adjustment, gtk_adjustment_get_upper(adjustment));
}

void updateProgress(double fraction) {
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), fraction);
     //  For text on the progress bar:
    char text[32];
    sprintf(text, "%.0f%%", fraction * 100);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), text);

}

// Callback function for the "Convert" button
void on_convert_button_clicked(GtkWidget *button, gpointer user_data) {
    const char *input_dir = gtk_entry_get_text(GTK_ENTRY(input_dir_entry));
    const char *output_dir = gtk_entry_get_text(GTK_ENTRY(output_dir_entry));
    const char *format = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(format_combo));

    if (input_dir && output_dir && format) {
        convertFilesBatch((char*)input_dir, (char*)output_dir, (char*)format);
    } else {
        showMessage("Please enter input and output directories and select a format.");
    }
    //The program crashes if these are freed.GTK handles the memory.
    //free(input_dir);
    //free(output_dir);
    //free(format);
}

// Callback for the "View Logs" button
void on_view_logs_button_clicked(GtkWidget *button, gpointer user_data) {
     viewLogs();
}


// Create the main window
GtkWidget *createMainWindow() {
    GtkWidget *window, *vbox, *hbox, *label, *button;
    GtkWidget *scrolled_window;
    GtkWidget *format_label;
    GtkComboBoxText *combo;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "File Converter");
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Input and Output Directories
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    label = gtk_label_new("Input Directory:");
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
    input_dir_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), input_dir_entry, TRUE, TRUE, 5);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    label = gtk_label_new("Output Directory:");
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
    output_dir_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), output_dir_entry, TRUE, TRUE, 5);

     // Format selection
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    format_label = gtk_label_new("Format:");
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(hbox), format_label, FALSE, FALSE, 5);
    format_combo = gtk_combo_box_text_new_with_entry();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(format_combo), "txt");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(format_combo), "csv");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(format_combo), "jpg");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(format_combo), "png");
    gtk_combo_box_set_active(GTK_COMBO_BOX(format_combo), 0);
    gtk_box_pack_start(GTK_BOX(hbox), format_combo, TRUE, TRUE, 5);


    // Convert Button
    button = gtk_button_new_with_label("Convert Files");
    gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 10);
    g_signal_connect(button, "clicked", G_CALLBACK(on_convert_button_clicked), NULL);

     // Progress Bar
    progress_bar = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(vbox), progress_bar, FALSE, FALSE, 10);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), "0%");


    // Log Text View
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_hexpand(scrolled_window, TRUE);
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 10);

    log_text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(log_text_view), FALSE);
    gtk_container_add(GTK_CONTAINER(scrolled_window), log_text_view);

     // View Logs Button
    button = gtk_button_new_with_label("View Logs");
    gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 10);
    g_signal_connect(button, "clicked", G_CALLBACK(on_view_logs_button_clicked), NULL);


    return window;
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    window = createMainWindow();
    gtk_widget_show_all(window);
    gtk_main();
    return 0;
}

