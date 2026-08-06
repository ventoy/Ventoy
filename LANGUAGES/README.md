## Language Package Guidelines

- **File encoding:** UTF-8

- **Language name format:**  
  Language names must follow this format:

Where:
1. `Language-` is a fixed prefix of **9 characters**
2. `XXX` is the language name in **English**
3. One space character (ASCII `0x20`)
4. Left parenthesis `(` (ASCII `0x28`)
5. `YYY` is the language name in the **native language**
6. Right parenthesis `)` (ASCII `0x29`)

- **String translation rules:**
- All strings must be explicitly defined
- The marker `#@` will be replaced with `\r\n`

- **Sorting rule:**  
All languages must be sorted **lexicographically by name**

---

> **Note**
>
> After adding a new language package in `LANGUAGES/languages.json`, please also add a corresponding entry
> to the supported languages list file located at:
>
>  [`LANGUAGES/languages_list.md`](languages_list.md)
>
> This helps keep the language support documentation complete and up to date.
