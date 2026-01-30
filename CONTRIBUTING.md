# Contributing to Mc-special-edition / Doprinos projektu

[English](#english) | [Srpski/Hrvatski/Bosanski](#srpskihrvatskibosanski)

---

## English

### How to Upload Files to This Repository

There are several ways to upload files to this repository. Choose the method that works best for you:

#### Method 1: GitHub Web Interface (Easiest for Beginners)

1. **Navigate to the repository** on GitHub: https://github.com/digit99-source/Mc-special-edition
2. **Click "Add file"** button in the repository
3. Choose **"Upload files"**
4. **Drag and drop** your files or click "choose your files" to browse
5. Add a **commit message** describing what you're uploading
6. Click **"Commit changes"**

**Note**: You need write access to the repository. If you don't have it, you'll be prompted to fork the repository and create a pull request.

#### Method 2: Git Command Line

If you have Git installed on your computer:

```bash
# Clone the repository (first time only)
git clone https://github.com/digit99-source/Mc-special-edition.git
cd Mc-special-edition

# Add your files to the repository
cp /path/to/your/files/* .
git add .

# Commit your changes
git commit -m "Add new files"

# Push to GitHub
git push origin main
```

#### Method 3: GitHub Desktop (User-Friendly GUI)

1. **Download and install** [GitHub Desktop](https://desktop.github.com/)
2. **Clone the repository**: File → Clone Repository → Enter URL
3. **Add your files** to the local repository folder
4. **Commit** the changes in GitHub Desktop
5. **Push** to origin

#### Method 4: Create a Pull Request (Recommended for Contributors)

If you don't have direct write access:

1. **Fork** the repository by clicking the "Fork" button on GitHub
2. **Clone your fork** to your computer
3. **Create a new branch**: `git checkout -b my-new-feature`
4. **Add and commit** your files
5. **Push** to your fork: `git push origin my-new-feature`
6. **Create a Pull Request** on GitHub from your fork to the main repository

### File Organization

- Keep files organized in appropriate directories
- Use descriptive file names
- Include README files in directories to explain their contents

### Questions?

If you have questions or need help, please:
- Open an issue on GitHub
- Contact the repository maintainer

---

## Srpski/Hrvatski/Bosanski

### Kako da Upload-ujem Fajlove u Ovaj Repozitorijum

Postoji nekoliko načina da upload-ujete fajlove u ovaj repozitorijum. Izaberite metod koji vam najbolje odgovara:

#### Metod 1: GitHub Web Interfejs (Najlakše za Početnike)

1. **Otvorite repozitorijum** na GitHub-u: https://github.com/digit99-source/Mc-special-edition
2. **Kliknite na "Add file"** dugme u repozitorijumu
3. Izaberite **"Upload files"**
4. **Prevucite i ispustite** vaše fajlove ili kliknite "choose your files" da pretražite
5. Dodajte **commit poruku** koja opisuje šta upload-ujete
6. Kliknite **"Commit changes"**

**Napomena**: Potreban vam je pristup za pisanje u repozitorijum. Ako ga nemate, biće vam ponuđeno da fork-ujete repozitorijum i kreirate pull request.

#### Metod 2: Git Komandna Linija

Ako imate Git instaliran na računaru:

```bash
# Klonirajte repozitorijum (samo prvi put)
git clone https://github.com/digit99-source/Mc-special-edition.git
cd Mc-special-edition

# Dodajte vaše fajlove u repozitorijum
cp /putanja/do/vasih/fajlova/* .
git add .

# Commit-ujte promene
git commit -m "Dodaj nove fajlove"

# Push-ujte na GitHub
git push origin main
```

#### Metod 3: GitHub Desktop (Korisnički Prijateljski GUI)

1. **Preuzmite i instalirajte** [GitHub Desktop](https://desktop.github.com/)
2. **Klonirajte repozitorijum**: File → Clone Repository → Unesite URL
3. **Dodajte vaše fajlove** u lokalnu repozitorijum fasciklu
4. **Commit-ujte** promene u GitHub Desktop aplikaciji
5. **Push-ujte** na origin

#### Metod 4: Kreirajte Pull Request (Preporučeno za Saradnike)

Ako nemate direktan pristup za pisanje:

1. **Fork-ujte** repozitorijum klikom na "Fork" dugme na GitHub-u
2. **Klonirajte vaš fork** na vaš računar
3. **Kreirajte novu granu**: `git checkout -b moja-nova-funkcionalnost`
4. **Dodajte i commit-ujte** vaše fajlove
5. **Push-ujte** na vaš fork: `git push origin moja-nova-funkcionalnost`
6. **Kreirajte Pull Request** na GitHub-u sa vašeg fork-a ka glavnom repozitorijumu

### Organizacija Fajlova

- Držite fajlove organizovane u odgovarajućim direktorijumima
- Koristite opisna imena fajlova
- Uključite README fajlove u direktorijumima da objasnite njihov sadržaj

### Pitanja?

Ako imate pitanja ili vam je potrebna pomoć:
- Otvorite issue na GitHub-u
- Kontaktirajte održavaoca repozitorijuma
