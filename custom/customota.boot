
#Select boot.img file to flash

ui_print(" ");
ui_print("Select boot.img to flash...");
ui_print(" ");
package_extract_file("install-prep.sh", "/tmp/install-prep.sh");
run_program("/sbin/sh", "/tmp/install-prep.sh");

ifelse(
    file_getprop("/tmp/property.kernel.oc","kernel.oc") == "oc",
    (
        ui_print("Flashing OC kernel...");
        ui_print(" ");
        package_extract_file("boot.img-oc", "/dev/block/mmcblk0p3");
    ),
    (
        ifelse(
             file_getprop("/tmp/property.kernel.oc","kernel.oc") == "oc_ultra",
             (
                 ui_print("Flashing OC ULTRA kernel...");
                 ui_print(" ");
                 package_extract_file("boot.img-oc_ultra", "/dev/block/mmcblk0p3");
             ),
             (
                 ui_print("Flashing Standard kernel...");
                 ui_print(" ");
                 package_extract_file("boot.img", "/dev/block/mmcblk0p3");
             )
        );
    )
);

