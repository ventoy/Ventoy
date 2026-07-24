
# Ventoy Secure Boot Policy

## Why shim
Ventoy is based on grub2 which is in GPL license, so it will not directly get signed with UEFI Certificate.  
Like most Linux distros, Ventoy grub2 must be launched by shim when Secure Boot is enabled.  
Different is that, Ventoy does not have its own shim and must make use of a third-part shim.  
When boot Ventoy with Secure Boot enabled, the shim will not directly boot Ventoy grub because Ventoy grub is not signed with the shim embedded Certificate.
The shim will open the MokManager and we must enroll Ventoy's Secure Boot Key and reboot, then the shim will accept Ventoy grub and boot it.

## Which shim
I choose a shim file from Rocky Linux because it was signed with both UEFI CA 2011 and UEFI CA 2023.

## Policy
Now you can boot into Ventoy after you enroll Ventoy Secure Boot Key.  
Ventoy provides two secure boot policies.  
1. Fully bypass secure boot.  
   This mode will bypass secure boot which means it will boot any EFI files without check.
   This mode enables all features of Ventoy and is the most convenient to use.
   However, it also carries certain risks, as the Secure Boot mechanism is effectively bypassed.
   You must therefore be fully aware of the risks involved and decide whether to use this mode according to your actual circumstances.
  
3. Follow UEFI firmware secure boot policy.  
   This mode will use the original UEFI secure boot policy to check every EFI file before boot it.  
   This means you can only boot these EFI files which are signed with UEFI CA (e.g. Windows bootmgr, another shim).
   In this mode, certain features of Ventoy are disabled (such as grub2boot mode, insmod, etc.), but this mode is the most secure.

Policy 1 is the default policy by now.  

If you want to use policy 2, you can set `VTOY_SECURE_BOOT_POLICY` to `1` in Ventoy global control plugin as follows:  
(PS: `VTOY_SECURE_BOOT_POLICY` value `0` and `1` correspond to Policy 1 and Policy 2 respectively)

```
{
    "control":[
        { "VTOY_SECURE_BOOT_POLICY": "1" }
    ]
}
```


