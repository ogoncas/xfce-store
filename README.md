# Xfce-Store

Documentação do projeto Loja de Aplicativos para XFCE (C + GTK3), versão 1.2

## 0. Novidades da versão 1.2

- Uma segunda execução da loja apenas traz a janela existente para a frente, em vez de abrir outra.
- Fechar a configuração inicial pelo "X" não ativa mais o Flathub nem grava nada: só o botão **Continuar** aplica as escolhas.
- Remover um pacote Debian agora simula a operação antes (`apt-get -s remove`) e pede confirmação quando a remoção arrastaria outros pacotes.
- Os apps Debian passam a vir do catálogo AppStream (DEP-11), com nome legível, resumo e categoria reais. O Debtags continua como complemento.
- A busca complementar no apt escapa a consulta (expressões regulares e termos que começam com `-`).
- O catálogo Flatpak não é mais carregado duas vezes na abertura, e o `flatpak update --appstream` só roda quando o catálogo tem mais de 24 horas (ou no `F5`).
- Ordenação por nome com regras do idioma (acentos), ícones sem colisão no cache, pacotes retidos (`hi`) contam como instalados, arquitetura e diretórios do Flatpak mais fiéis.
- Comandos cuja saída é interpretada rodam com `LC_ALL=C`; todos rodam com a entrada padrão fechada.
- A janela não fecha enquanto uma instalação ou remoção está em andamento.
- Novo ID de aplicativo e novo nome do `.desktop`; o `Makefile` ganhou `uninstall` e `.PHONY`.

## 1. Visão geral

O **xfce-store** é uma loja de aplicativos gráfica para Debian e derivados (Xubuntu, Linux Mint XFCE, MX etc.), escrita em C com GTK3 e pensada para se integrar ao XFCE. Ela reúne em uma única lista:

- apps **Flatpak** de qualquer repositório configurado (Flathub e outros), nos escopos de usuário e de sistema;
- aplicativos gráficos dos **repositórios Debian (apt)**.

O projeto tem três arquivos:

| Arquivo | Função |
| --- | --- |
| `xfce-store.c` | Todo o código da aplicação (interface, catálogos, instalação, configurações, idiomas). |
| `Makefile` | Compila o programa, instala e desinstala o binário e o atalho de menu. |
| `io.github.xfcestore.Store.desktop` | Entrada de menu que registra a loja no XFCE. |

A aplicação usa apenas GTK3, GLib e GIO. Não depende de bibliotecas de pacotes: ela conversa com as ferramentas de linha de comando do sistema (`flatpak`, `apt-cache`, `dpkg-query`, `apt-get`, `pkexec`) usando subprocessos assíncronos.

## 2. Dependências

**Para compilar**

```
sudo apt install build-essential pkg-config libgtk-3-dev
```

**Para executar**

- `flatpak` (opcional; sem ele só a parte Debian funciona);
- `pkexec` e o serviço polkit. Os nomes dos pacotes dependem da versão:
  - Debian 12 e Ubuntu 24.04 ou mais novos: `polkitd` e `pkexec`;
  - versões mais antigas (Debian 11, Ubuntu 22.04, Linux Mint 21 e anteriores): `policykit-1`;
- um **agente polkit** em execução na sessão, como `policykit-1-gnome` ou `xfce-polkit`, para o diálogo de senha;
- `appstream` (recomendado): fornece nomes, resumos, categorias e ícones dos apps Debian.

## 3. xfce-store.c

### 3.1 Estrutura geral

O arquivo é organizado em blocos, nesta ordem:

1. **Tipos e constantes**: a estrutura `Entry` (um app: nome, id, descrição, ícone, repositório, fonte, categorias e chave de ordenação), o ID da aplicação e a URL do Flathub.
2. **Idiomas (i18n)**.
3. **Categorias**.
4. **Cache em disco**.
5. **Catálogo Flatpak** (lido em uma thread de trabalho).
6. **Catálogo Debian** (lido em outra thread): AppStream DEP-11 e Debtags.
7. **Estado da aplicação e utilitários** (estrutura `App`, execução de comandos, barra de progresso, log).
8. **Lista de apps** (ícones, texto, filtros).
9. **Carregamento de dados** (catálogos, apps instalados, repositórios Flatpak).
10. **Instalar, remover e atualizar**.
11. **Ações do menu**.
12. **Busca e navegação**.
13. **Configuração, configuração inicial e janela de Configurações**.
14. **Interface** e a função `main`.

### 3.2 Fontes de dados

**Flatpak.** A loja não usa `flatpak search`. Ela lê diretamente o catálogo AppStream que o próprio flatpak mantém em disco:

- usuário: `~/.local/share/flatpak/appstream/<repositório>/<arquitetura>/active/appstream.xml.gz` (ou o diretório indicado por `FLATPAK_USER_DIR`);
- sistema: `/var/lib/flatpak/appstream/<repositório>/<arquitetura>/active/appstream.xml.gz` (ou o diretório indicado por `FLATPAK_SYSTEM_DIR`).

A arquitetura segue os nomes do Flatpak: `x86_64`, `aarch64`, `i386` (para `i686` e similares) e `arm` (para `armv7l`, `armv8l` etc.). Instalações personalizadas listadas por `flatpak --installations` ainda não são lidas.

O arquivo é descompactado e lido em fluxo por um parser XML do GLib, em uma thread separada. São aceitos componentes do tipo `desktop-application` (e `desktop`). De cada componente são extraídos o id (sem o sufixo `.desktop`), o nome e o resumo (somente os campos sem `xml:lang`, ou seja, em inglês), as categorias e o ícone em cache de 64 px. Apps sem categoria reconhecida entram em Utilitários. Por isso a busca no Flatpak é instantânea e funciona offline.

Se o mesmo repositório existir nos escopos de usuário e de sistema, é usado apenas o de usuário.

**Debian.** A thread do apt monta o catálogo em duas etapas:

1. **AppStream (DEP-11).** Lê os arquivos `*.yml.gz` (ou `*.yml`) de `/var/lib/swcatalog/yaml`, `/var/lib/app-info/yaml` e equivalentes em `/usr/share`. Entram os componentes do tipo `desktop-application`, com nome e resumo (campos `C`, sem tradução), pacote e categorias do freedesktop. É a fonte principal e funciona em Debian, Ubuntu e derivados, desde que o pacote `appstream` esteja instalado e o `apt update` já tenha sido executado.
2. **Debtags.** Executa `apt-cache dumpavail` e lê a saída parágrafo por parágrafo. Pacotes que o Debtags marca como programa gráfico (`role::program` com `interface::x11` ou alguma `uitoolkit::`) e que não vieram do AppStream são acrescentados, com o nome do pacote como título e a categoria derivada da seção. Fora do Debian o campo `Tag:` pode não existir; nesse caso só o AppStream alimenta a lista.

Um pacote nunca aparece duas vezes. Apps sem categoria reconhecida entram em Utilitários.

Os ícones do Debian vêm do cache AppStream do sistema (`/var/lib/swcatalog/icons`, `/var/lib/app-info/icons` e equivalentes em `/usr/share`), cujos arquivos se chamam `<pacote>_<ícone>.png`. Sem esses arquivos, a loja tenta um ícone do tema com o mesmo nome do pacote e, por fim, o ícone genérico `package-x-generic`.

**Busca complementar no apt.** Com 3 ou mais letras digitadas e a fonte diferente de "Flatpak", a loja também executa `apt-cache search` e acrescenta até 100 pacotes que não estão no catálogo gráfico (ferramentas de terminal, por exemplo). Cada palavra digitada vira um padrão escapado (assim `c++` não gera erro de expressão regular e um termo iniciado por `-` não é tomado como opção), e o pacote precisa casar com todas as palavras. Os resultados aparecem somente na categoria "Todos os apps". Um contador de geração descarta resultados de buscas antigas que chegam atrasados.

### 3.3 Cache em disco

Os catálogos ficam em `~/.cache/xfce-store/` (`flathub.tsv` e `apt.tsv`), em texto separado por tabulações. A primeira linha é `v3<TAB><chave>` e cada linha seguinte tem as colunas nome, id, descrição, ícone, repositório e categorias. Caches de versões anteriores (`v2`) são ignorados e reconstruídos.

A chave decide quando o cache vale:

- **Flatpak:** soma da data de modificação do `appstream.xml.gz` de cada repositório ativo mais um valor calculado a partir do escopo e do nome do repositório;
- **Debian:** data de `/var/cache/apt/pkgcache.bin` somada às datas das pastas de ícones do AppStream e de cada arquivo de catálogo DEP-11.

Se a chave não bate, o catálogo é reconstruído e o cache é regravado. Depois da primeira execução, a lista aparece quase instantaneamente.

### 3.4 Categorias

São 11 entradas na barra lateral: Todos os apps, Instalados, Internet, Multimídia, Escritório, Gráficos, Jogos, Desenvolvimento, Educação e Ciência, Utilitários e Sistema. Cada uma usa um ícone simbólico; se o tema não tiver a versão simbólica, o app usa o ícone normal.

Cada app guarda suas categorias em uma máscara de bits. As categorias do freedesktop (`Network`, `AudioVideo`, `Game` etc.) são convertidas para essas 9 categorias temáticas; "Todos os apps" e "Instalados" são filtros especiais. Para pacotes que só têm a seção do Debian, a tabela de seções cobre jogos, gráficos, som e vídeo, rede, escritório, desenvolvimento (incluindo linguagens), ciência e educação e administração; seções como `x11`, `gnome`, `kde`, `xfce` e `editors` ficam em Utilitários de propósito. "Instalados" mostra apenas apps que existem no catálogo e estão instalados, então pacotes Debian sem interface gráfica não aparecem ali.

### 3.5 Lista, filtros e desempenho

- O modelo da lista guarda apenas um ponteiro para cada `Entry`. Ícone, texto, fonte e status são calculados na hora de desenhar a linha. Por isso a lista é esvaziada sempre antes de qualquer `Entry` ser liberado (ao trocar de catálogo ou de busca).
- A lista completa (Flatpak mais Debian) é ordenada por nome **uma única vez**, quando um catálogo termina de carregar, usando chaves de ordenação do idioma (`g_utf8_collate_key`, calculadas uma vez por app). Nomes com acento ficam no lugar certo. Trocar de categoria ou digitar só filtra, sem reordenar.
- Durante a reconstrução da lista, o modelo é desligado da árvore, o que acelera muito a inserção de milhares de linhas.
- O texto em duas linhas (nome em negrito e descrição) é montado na primeira vez que a linha é desenhada e guardado.
- Os ícones de 40 px são carregados sob demanda e guardados em um cache em memória. Apps sem ícone próprio usam o id como chave, então nenhum reaproveita o ícone de outro.
- O filtro de texto procura, sem diferenciar maiúsculas e minúsculas, no nome, no id e na descrição.

### 3.6 Apps instalados

- Flatpak: `flatpak list --app --columns=application,installation`, que informa também se o app está no escopo `user` ou `system`.
- Debian: `dpkg-query` filtrando os pacotes com estado `ii` (instalado) ou `hi` (instalado e retido).

Esses comandos rodam com `LC_ALL=C`, para que a saída tenha sempre o mesmo formato, e linhas de cabeçalho são ignoradas. O resultado marca "✔ Instalado" na lista e define se o botão mostra Instalar ou Remover. A lista é atualizada depois de cada operação.

### 3.7 Instalar, remover e atualizar

Todas as operações rodam de forma assíncrona. Enquanto uma ação está em andamento, o botão fica desativado, a barra de progresso pulsa e a barra de status descreve o que está acontecendo. A saída dos comandos vai para o log. Todos os subprocessos têm a entrada padrão fechada, então nenhum comando fica esperando uma resposta.

| Ação | Comando |
| --- | --- |
| Instalar app Flatpak | `flatpak install <--user ou --system> --noninteractive -y <repositório> <id>` |
| Remover app Flatpak | `flatpak uninstall <--user ou --system> --noninteractive -y <id>` |
| Instalar pacote Debian | `pkexec env DEBIAN_FRONTEND=noninteractive apt-get install -y <pacote>` |
| Simular remoção Debian | `apt-get -s remove <pacote>` (sem privilégios) |
| Remover pacote Debian | `pkexec env DEBIAN_FRONTEND=noninteractive apt-get remove -y <pacote>` |
| Atualizar lista de pacotes | `pkexec env DEBIAN_FRONTEND=noninteractive apt-get update` (depois recarrega o catálogo Debian) |
| Atualizar catálogos Flatpak | `flatpak update --appstream --noninteractive` |

O escopo de instalação do Flatpak segue o repositório: se ele estiver ativo no usuário, usa `--user`; senão, `--system`. Na remoção, vale o escopo em que o app está instalado. Operações no escopo `system` pedem autorização pelo polkit do próprio flatpak, e as do apt usam `pkexec`. Como o apt é chamado através do `env`, o diálogo de senha do polkit menciona `/usr/bin/env`; isso é esperado. A variável `DEBIAN_FRONTEND=noninteractive` evita que perguntas do debconf travem a operação, já que não há terminal.

**Confirmação na remoção Debian.** Antes de remover, a loja simula a operação. Se a simulação mostrar que outros pacotes também seriam removidos (dependências reversas, metapacotes), aparece um diálogo listando até 30 deles, com **Cancelar** como opção padrão. Só depois da confirmação o `pkexec` é executado. Se a simulação falhar, nada é removido e o motivo vai para o log.

**Fechar a janela durante uma operação.** Enquanto uma instalação, remoção ou atualização de pacotes está em andamento, a janela não fecha: um aviso pede para aguardar. Isso evita interromper o `apt` ou o `dpkg` no meio do trabalho.

Se uma operação falhar, o log é expandido (quando está visível). Se o log estiver oculto, a barra de status avisa que a operação falhou e sugere ativar o log nas Configurações.

### 3.8 Repositórios Flatpak

Na inicialização, a loja executa `flatpak remotes --columns=name,url,options` (com `LC_ALL=C`) e interpreta a coluna de opções para saber o escopo (`user` ou `system`) e se o repositório está desativado. O resultado alimenta a janela de Configurações e define quais catálogos carregar. Esse mecanismo corrige o caso de o Flathub estar ativo apenas no sistema.

Depois de conhecer os repositórios, a loja carrega o catálogo que já está em disco e só executa `flatpak update --appstream` se algum catálogo estiver ausente ou tiver mais de 24 horas. O `F5` força a atualização. Se duas cargas do catálogo Flatpak terminarem fora de ordem, a mais antiga é descartada.

O Flathub **não é adicionado automaticamente**. Ele só é ativado quando o usuário escolhe isso na configuração inicial ou clica em "Ativar o Flathub" nas Configurações. Nesses casos a loja executa `flatpak remote-add --user --if-not-exists flathub <URL>`.

### 3.9 Janela principal

- **Barra superior:** campo de busca (a busca é feita enquanto você digita), seletor de fonte (Todas as fontes, Flatpak, Debian) e botão de menu com três pontinhos.
- **Esquerda:** lista de categorias com ícones simbólicos.
- **Centro:** lista de apps com ícone de 40 px, nome em negrito, descrição, fonte (Flathub, nome do repositório ou Debian) e status.
- **Barra inferior:** informações do app selecionado (nome, fonte e id), barra de progresso (visível só quando algo está rodando) e o botão Instalar/Remover.
- **Log:** área expansível "Detalhes (log)", que pode ser ocultada nas Configurações.

O visual usa apenas widgets GTK padrão, sem cores ou temas próprios, e por isso acompanha o tema do XFCE. A loja é de instância única: abrir de novo apenas traz a janela existente para a frente.

**Menu de três pontinhos:** Recarregar catálogos, Atualizar lista de pacotes, Configurações e Sobre.

**Atalhos:** `F5` recarrega os catálogos, `Ctrl+F` foca a busca e `Ctrl+,` abre as Configurações.

### 3.10 Janela de Configurações

É uma janela separada, ligada à janela principal, com três áreas:

1. **Flathub:** mostra se ele está ativado e, se não estiver, oferece o botão para ativá-lo.
2. **Repositórios Flatpak:** tabela com nome, URL e tipo (Usuário ou Sistema). É possível adicionar um repositório informando nome e URL de um arquivo `.flatpakrepo` (sempre no escopo do usuário) e remover o repositório selecionado, no escopo correto. Uma linha de mensagem mostra o resultado de cada operação. Remover um repositório que ainda tem apps instalados pode falhar, e o motivo aparece nessa linha.
3. **Interface:** a opção "Mostrar a área de logs na tela inicial" (aplicada na hora) e o seletor de **Idioma** (Automático, English ou Português (Brasil)).

Sem o flatpak instalado, a área de repositórios fica desativada com um aviso.

### 3.11 Configuração inicial

Na primeira execução, depois que a loja conhece os repositórios existentes, aparece uma janela com duas opções: ativar o Flathub (marcada por padrão) e mostrar a área de logs na tela inicial (desmarcada). Se o Flathub já estiver ativo, ou se o flatpak não estiver instalado, a janela informa isso e desativa a primeira opção.

Só o botão **Continuar** aplica e grava as escolhas (e marca `setup_done`). Fechar a janela pelo "X" não muda nada e não grava nada: a pergunta volta na próxima vez que a loja for aberta. Em uma mesma execução a janela aparece no máximo uma vez.

### 3.12 Arquivo de configuração

Fica em `~/.config/xfce-store/config.ini`, no grupo `[general]`:

| Chave | Significado |
| --- | --- |
| `setup_done` | `true` depois que a configuração inicial foi concluída com **Continuar**. |
| `show_log` | Mostra ou oculta a área de logs na janela principal. |
| `language` | `auto`, `en` ou `pt`. |

### 3.13 Idiomas

O inglês é o idioma-base e também o fallback para qualquer outra língua. As traduções para português do Brasil ficam em uma tabela no início do arquivo, e as mensagens do programa são escritas em inglês e passam por uma função de tradução. Com `language=auto`, a loja usa português se o idioma do sistema começar com `pt` e inglês nos demais casos. Essa escolha é deliberada: o português do Brasil é usado também em `pt_PT`, por estar mais próximo dele do que o inglês. Escolher outro idioma nas Configurações só vale após reabrir a loja.

Os textos do próprio GTK (por exemplo, os botões do diálogo "Sobre") continuam seguindo o idioma do sistema. Para incluir outro idioma, é preciso adicionar uma nova tabela de traduções e ajustar a função de inicialização de idiomas.

### 3.14 Arquitetura de execução

- A interface roda na thread principal do GTK.
- Os catálogos são lidos por tarefas (`GTask`) em threads de trabalho. Elas só mexem em dados locais e devolvem o resultado à thread principal. Cada carga recebe um número de geração; resultados de cargas superadas são descartados.
- Os comandos externos rodam como subprocessos assíncronos (`GSubprocess`, criados por um `GSubprocessLauncher`) e chamam uma função de retorno quando terminam. Os que têm a saída interpretada usam `LC_ALL=C`.
- Um contador de ocupação controla a barra de progresso, que só aparece enquanto há trabalho em andamento.
- A aplicação usa o ID `io.github.xfcestore.Store`. Troque-o por um ID sob um domínio ou conta que você controle (em `APP_ID`, no `.c`, e no nome do `.desktop`, junto com a variável `APPID` do `Makefile`).

### 3.15 Limitações conhecidas

- Os resumos dos apps Flatpak vêm do catálogo e ficam em inglês; o mesmo vale para os resumos do AppStream do Debian.
- Pacotes Debian que só aparecem pelo Debtags usam o nome do pacote como título, e a categoria deles é aproximada, pois deriva da seção.
- Sem o pacote `appstream` (ou fora de Debian e derivados com DEP-11), a lista Debian depende do Debtags, que pode não existir em Ubuntu e Mint.
- Apps instalados a partir de repositórios Flatpak que não estejam ativos não aparecem na lista de instalados.
- Instalações Flatpak personalizadas (`flatpak --installations`) não são lidas.
- A saída dos comandos é mostrada no log quando o comando termina, e não linha a linha.
- Não há página de detalhes, capturas de tela nem aba de atualizações pendentes.
- Os repositórios adicionados pela loja são sempre do escopo do usuário.
- A licença exibida no diálogo "Sobre" é GPL-3.0; ela pode ser trocada em `act_about()`.

## 4. Makefile

O `Makefile` automatiza a compilação, a instalação e a remoção.

**Variáveis** (todas podem ser sobrescritas na linha de comando):

| Variável | Valor padrão | Uso |
| --- | --- | --- |
| `CC` | `gcc` | Compilador. |
| `CFLAGS` | `-O2 -Wall` | Otimização e avisos. |
| `PKG` | `gtk+-3.0` | Pacote consultado pelo `pkg-config`. |
| `PREFIX` | `/usr/local` | Raiz da instalação. |
| `APPID` | `io.github.xfcestore.Store` | ID da aplicação, usado no nome do `.desktop`. |

**Alvos**

- `make` (alvo `all`, que gera `xfce-store`): compila `xfce-store.c` com as flags e bibliotecas fornecidas por `pkg-config --cflags --libs gtk+-3.0`. Nenhuma biblioteca adicional é necessária.
- `make install`: instala `xfce-store` em `$(PREFIX)/bin` (permissão 755) e `$(APPID).desktop` em `$(PREFIX)/share/applications` (permissão 644). Aceita `DESTDIR` para empacotamento.
- `make uninstall`: remove os dois arquivos instalados.
- `make clean`: remove o binário gerado.

Os alvos `all`, `install`, `uninstall` e `clean` estão declarados em `.PHONY`.

**Exemplos**

```
make                                  # compila
sudo make install                     # instala em /usr/local
sudo make PREFIX=/usr install         # instala em /usr
sudo make uninstall                   # remove o que foi instalado em /usr/local
make CFLAGS="-O2 -Wall -Wextra"       # compila com mais avisos
```

## 5. io.github.xfcestore.Store.desktop

É a entrada de menu no padrão freedesktop. Depois de instalada, a loja aparece nos menus do XFCE e pode ser buscada pelo lançador. O nome do arquivo é o mesmo ID usado pela aplicação, que é como os ambientes associam a janela ao atalho.

| Chave | Valor | Explicação |
| --- | --- | --- |
| `Type` | `Application` | Indica que é um aplicativo. |
| `Name` | `Software Store` | Nome em inglês (fallback). |
| `Name[pt_BR]` | `Loja de Aplicativos` | Nome em português do Brasil. |
| `Comment` / `Comment[pt_BR]` | Descrição curta | Texto da dica do item no menu, em cada idioma. |
| `Exec` | `xfce-store` | Comando executado; o binário precisa estar no `PATH` (`/usr/local/bin` normalmente está). |
| `Icon` | `system-software-install` | Ícone do tema, o mesmo usado na janela. |
| `Terminal` | `false` | Não abre terminal. |
| `StartupNotify` | `true` | Permite o indicador de carregamento ao abrir pelo menu. |
| `StartupWMClass` | `xfce-store` | Associa a janela ao atalho no painel, no X11 (é o nome do binário). |
| `Categories` | `System;PackageManager;GTK;` | Coloca a loja na seção de sistema e gerenciadores de pacotes do menu. |
| `Keywords` | `apps;flatpak;flathub;apt;store;loja;instalar;` | Palavras que ajudam a encontrá-la na busca do menu. |

Se o menu não atualizar, rode `update-desktop-database ~/.local/share/applications` ou, para instalação global, `sudo update-desktop-database`. Quem tinha a versão 1.1 instalada deve remover o antigo `xfce-store.desktop` de `$(PREFIX)/share/applications`.

## 6. Como os arquivos se conectam

```
xfce-store.c  --(make, pkg-config gtk+-3.0)-->  binário xfce-store
binário       --(make install)-->  $(PREFIX)/bin/xfce-store
io.github.xfcestore.Store.desktop --(make install)--> $(PREFIX)/share/applications/
menu do XFCE  --(Exec=xfce-store)-->  abre a loja
```

Fluxo de dados em execução:

```
flatpak remotes ──► repositórios (usuário/sistema) ──► appstream.xml.gz ──┐
AppStream DEP-11 (yml.gz) + apt-cache dumpavail/Debtags + ícones ─────────┤
                                                                          ▼
                                    cache em ~/.cache/xfce-store  ──►  lista única ordenada
                                                                          ▼
                          filtros (categoria, fonte, busca) ──► lista na tela ──► instalar/remover
```

## 7. Instalação e primeiro uso

```
sudo apt install build-essential pkg-config libgtk-3-dev flatpak
sudo apt install polkitd pkexec         # Debian 12 / Ubuntu 24.04 ou mais novos
sudo apt install policykit-1            # em versões mais antigas, no lugar da linha acima
sudo apt install policykit-1-gnome      # ou xfce-polkit, se não houver agente polkit
sudo apt install appstream              # recomendado: nomes, categorias e ícones dos apps Debian
sudo apt update                         # baixa os catálogos AppStream
cd xfce-store
make
./xfce-store                            # teste direto
sudo make install                       # instala o binário e o atalho de menu
```

Na primeira abertura, responda à configuração inicial com **Continuar**. A primeira leitura dos catálogos é mais lenta; depois ela usa o cache.

## 8. Solução de problemas

| Sintoma | O que fazer |
| --- | --- |
| Lista vazia no Flatpak | Verifique se há repositórios ativos em Configurações. Rode `flatpak remotes` e `flatpak update --appstream`. |
| Lista vazia no Debian | Instale `appstream` e rode `sudo apt update`. Para conferir, veja se existem arquivos em `/var/lib/swcatalog/yaml` (ou `/var/lib/app-info/yaml`) e rode `apt-cache dumpavail \| head`. |
| Apps Debian com o nome do pacote em vez do nome do app | O app veio só do Debtags; instale `appstream` e rode `sudo apt update`. |
| Apps Debian sem ícone | Instale o pacote `appstream` e rode `sudo apt update`. |
| Senha não é pedida | Falta um agente polkit na sessão; instale `policykit-1-gnome` ou `xfce-polkit`. |
| A janela não fecha | Há uma instalação ou remoção em andamento; aguarde o fim. |
| Uma operação falhou | Ative a área de logs nas Configurações para ver a saída dos comandos. |
| Catálogo desatualizado | Use `F5` (Recarregar catálogos) ou apague `~/.cache/xfce-store/*.tsv`. |
| Rever a configuração inicial | Apague `~/.config/xfce-store/config.ini`. |
| Trocar o idioma | Escolha em Configurações e reabra a loja. |
