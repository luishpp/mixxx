# 🎧 Music Sync DJ — do zero ao set tocando

Você já tem as faixas. Este guia leva você da biblioteca até **o set rolando nos decks** — e te dá
os poucos gestos que transformam *"tá tocando"* em *"tá tocando do meu jeito"*.

Abrir o painel: **Opções → Music Sync**.

---

## ⚡ O caminho rápido

Da biblioteca ao set, em 4 movimentos:

0. ☑ **Enable Music Sync** (sem isso, tudo fica cinza).
1. **Analyze missing (Mixxx)** — só se houver faixa sem BPM/tom. Espere terminar.
2. **Read native analysis from library** — calcula tudo e grava.
3. **Generate sequence** — o set nasce.
4. Na janela do set: **Preview on decks** pra ouvir → ajuste o que quiser → **Run set** pra tocar
   (o set inteiro ou só um ato).

> 🔁 **Atualizou o módulo (build novo)?** Clique **Read native analysis from library** de novo. Só
> ele recalcula com as regras novas — reabrir o painel só relê o que já estava gravado.

---

## 🔌 Antes de tudo (setup, uma vez)

Um checklist rápido — se algo não toca, quase sempre é um destes:

- ☑ **Enable Music Sync** marcado.
- 🔊 **Saída de áudio de verdade**: *Preferences → Sound Hardware*. Use a **placa física** (fuja de
  virtuais tipo FxSound). Com **WASAPI**, a *Taxa de Amostragem* tem que ser **igual à do Windows**
  (geralmente 48000 Hz), senão dá `Invalid sample rate`.
- 🎚️ **Skin com 2 decks** (o preview usa deck 1 = A, deck 2 = B).
- 🎵 **Faixas analisadas** — BPM/beatgrid/tom **e waveform**. Sem waveform não há energia (é a causa
  nº 1 de "tudo virou Auto DJ fallback").

---

## 🎛️ Preparar a biblioteca

Três botões alimentam o motor. A ordem natural é **Analyze missing → Read native → (Generate)**.

### Analyze missing (Mixxx)

Pesca as faixas **sem beatgrid/BPM ou sem tom** e manda o **próprio Mixxx** analisar (em segundo
plano, prioridade baixa). Mostra `Analyzing i/N…`, trava os botões enquanto roda e, ao terminar,
**re-lê tudo sozinho**. Se não há nada pendente: *"Nothing to analyze"*.

> ⚠️ Ele só pesca quem **não tem beats/tom**. Uma faixa que tem beats e tom mas **não tem waveform**
> passa batido — e sem waveform não há energia. Nesse caso, clique-direito nela na biblioteca do
> Mixxx → **Analisar**.

### Read native analysis from library

**O botão que calcula tudo.** Percorre até **500** faixas e, para cada uma:

- pega o **waveform que o Mixxx já guardou** (não re-decodifica áudio);
- lê a análise nativa: **BPM, tom → Camelot, beatgrid, ReplayGain, intro/outro, duração**;
- calcula o resto: **energia, graves, frases, seções e janelas de entrada/saída**;
- **grava no sidecar** e repinta a tabela (e limpa do sidecar faixas que sumiram da biblioteca).

Ele **não** roda o analisador do Mixxx — só lê o que já existe. Faixa nunca analisada aparece como
`Analyzed = No`, sem dados (é aí que entra o *Analyze missing*).

**Use:** na primeira vez, ao adicionar faixas, depois do *Analyze missing* e **sempre que atualizar
o módulo**.

### Clear snapshots

Apaga **todos** os snapshots (com confirmação). Nada se perde de verdade — é tudo recomputável com
*Read native analysis*. Sua biblioteca, waveforms e cues do Mixxx ficam intactos (são outro banco).
Use para um **reset limpo** depois de refazer a biblioteca.

> O painel **relê os snapshots sozinho ao abrir** e se atualiza depois de cada ação, então não existe
> botão "recarregar" — reabrir já faz isso.

---

## 🧭 A tabela da biblioteca

Uma linha por faixa, pra você bater o olho no material:

| Coluna | O que é |
|---|---|
| BPM / Camelot / Key / Duration / ReplayGain | Análise nativa do Mixxx |
| **Analyzed** | `No` = faixa sem BPM/tom → rode *Analyze missing* |
| **Energy** | **posição relativa na SUA biblioteca (0–1)**: 1.00 = a mais forte que você tem |
| **Phrases** | nº de frases (16 compassos) tiradas do beatgrid |
| **Sections** | nº de seções (Intro/Groove/Build/Drop/Breakdown/Outro) |
| **Exit @** | início da melhor **janela de saída** (mm:ss) |

> **Energy é relativo de propósito**: uma jornada de energia é comparativa ("essa é mais forte que
> aquela"), então o módulo ranqueia a biblioteca inteira em vez de usar um volume absoluto que a
> música real nunca atinge.

---

## ✨ Generate sequence — o set nasce aqui

Precisa de **pelo menos 2 faixas analisadas**. Roda o otimizador (determinístico), gera **até 3
alternativas**, mostra a melhor e abre a janela do set.

Se suas faixas trazem o ato no comentário (`ATO 5 | ÂNCORA | ...`), entra a **curadoria híbrida** —
o roteiro manda, o motor arruma o resto:

- 📌 **A ordem do plano lidera.** A alternativa 1 é a sua numeração; as propostas do motor vêm
  depois, lado a lado, pra você comparar. **O motor informa, não decide.**
- 🎬 **A narrativa é lei:** nada de trocar faixa entre atos. Faixas `EXTRA` fecham o set.
- ⚓ **As âncoras ficam presas** na posição que o plano deu (marcadas `[locked]`) — *Space Explorers*
  fecha o Ato 1, *Under Control* fecha o Ato 4, e assim vai.
- 🎯 Cada ato é medido contra a **sua fatia** da jornada de energia; o peso da **harmonia** aperta
  nos atos melódicos e afrouxa nos flashes e no peak.

> Sem comentário de ato, cai na otimização global de sempre (e as âncoras só prendem quando **todas**
> as faixas do ato têm número). Quer tudo isso? Prepare os arquivos com número + comentário do §3.

---

## 🎚️ A mesa do set — janela "Generated sequence"

Aqui você molda o set. A janela é **modeless**: fica ao lado do Mixxx, que segue 100% usável;
**fechá-la não para o set** (só encerra um preview). Gerar de novo substitui a janela — nunca há
duas brigando pelos decks.

Cabeçalho: `Best of N alternative(s) — average compatibility X%, energy fit Y%`.

Cada linha é **uma faixa**:

| Coluna | O que é |
|---|---|
| **#** | posição no set |
| **Act** | o ato (1–7); `—` = extra, toca no fim |
| **Track** | artista - título, `[locked]` se for âncora presa |
| **Enter @** | onde **esta** faixa **entra** (pular intro) — editável |
| **Exit @** | onde **esta** faixa **entrega** para a próxima — editável |
| **Transition** | o tipo, com a origem: `(pair)`, `(act)` ou nada = automático |
| **Bars** | duração da virada, mesma marca de origem |
| **Match** | quão bem a passagem casa (harmonia, tempo, frase, energia, janela) |
| **Plan** | a explicação — ou o **aviso**, se houver |

A **1ª linha** tem `—` em Enter @ (a abertura não entra por transição, só começa). A **última** tem
`—` em Exit @/Transition/Bars (ela não entrega pra ninguém).

### Os gestos por faixa

Selecione a linha e mexa nos campos abaixo da tabela. **Cada linha é uma faixa, e tudo é sobre ela:**

```text
[Transition ▾]  Bars: [▾]  Enter @: [____] [→ drop]  Exit @: [____] [→ drop]
```

- ⏮️ **Enter @** — *onde a faixa começa.* Vazio = automático (que prefere a intro de baixa energia).
  Numa faixa de **intro longa**, aponte um ponto depois da intro (ex.: `0:48`): ela entra já batendo
  enquanto a anterior ainda toca — **mantém o clímax entre faixas**.
- ⏭️ **Exit @** — *onde a faixa entrega.* É o que controla **quanto ela toca**: quer um flash de 90 s
  (§9)? Ponha `1:30` e vira flash.
- 🪄 **→ drop** — *esperar o drop.* Ao lado de cada campo, esse botão joga o ponto pro **Drop
  detectado mais próximo** da própria faixa. **Exit @ → drop** = a faixa espera o clímax e entrega
  nele; **Enter @ → drop** = ela entra já no drop, sem intro. Vira um override normal (dá pra ajustar
  o mm:ss depois ou apagar pra voltar ao automático). Sem drop detectado, o rótulo avisa.
- 🎚️ **Transition / Bars** — *como e quão rápido vira* (detalhes abaixo).

> ⚙️ Os pontos sempre caem no compasso: a virada é **quantizada e beat-matchada** na hora de tocar.

### Transition — o que cada escolha faz nos decks

| Tipo | Nos decks | Bom para |
|---|---|---|
| **Automatic** *(padrão)* | o motor escolhe pelas regras e **explica** | deixe assim até algo te incomodar |
| **Crossfade** | A desce, B sobe, sem EQ | intro/outro limpos, modo seguro |
| **EQ Blend** | B entra com grave zerado; aos 60% os graves trocam | o feijão-com-arroz do mix harmônico |
| **Bass Swap** | igual, mas a troca de graves é no meio (50%) e mais decidida | kicks estáveis, frase clara |
| **Breakdown swap** | longo: grave de A sai cedo, filtro varre A, B cresce por baixo | tom incompatível (o §16 pede isto, **não** um corte) |
| **Filter** | B entra e A é varrida pelo filtro | com moderação — filtro não salva plano ruim |
| **Cut on phrase** | segura as duas e **troca seco na frase** | A termina com impacto, B começa com ataque |

- **Bars** = tamanho da virada (8/16/32/64). **Menos = mais rápida.** Automático mira 32, limitado
  pela janela (Cut nunca passa de 8).
- **Beat sync segue o plano, não o tipo:** quem sabe se os tempos casam é o planejador (casam quando
  o salto de tempo é ≤ ~8%). `Cut` e `Auto DJ fallback` nunca sincronizam.
- **Sem sync = sobreposição curta (anti-samba):** quando os tempos são distantes demais para casar
  (ex.: um salto pro trance), blendar as duas batidas geraria aquele *flam* ("samba"). Aí a virada
  fica **apertada**: a faixa que entra fica **muda até uma troca rápida** no fim da frase (com o
  filtro varrendo a que sai), então duas batidas desalinhadas nunca tocam alto juntas. Para uma
  entrada suave nesses casos, traga a nova faixa numa **intro sem batida** via **Enter @**.

> ⚠️ **Forçar o tipo errado é audível.** `Cut` num par harmônico e no mesmo tempo joga fora uma
> mescla boa; `Bass Swap` num par com 20% de diferença de tempo estica a faixa nova (foi assim que o
> Adagio saiu a −20% de pitch). O motor não te impede — **você mandou**.

### Ouvir: Preview on decks

O **Preview toca como a faixa selecionada ENTRA** — ela é a que chega, então começa no **Enter @**
dela e a faixa **de cima** entrega para ela. Ajuste → ouça → repita.

- Para ouvir como uma faixa **sai** (o Exit @/tipo/bars que você editou nela), dê Preview na **linha
  de baixo** — lá essa mesma virada é a "entrada" da próxima.
- A **abertura** não tem entrada, então o Preview começa a valer da 2ª linha em diante.
- **Repeat** re-posiciona e toca de novo. **Cancel preview** solta os decks sem cortar o áudio.
  **Mexer no crossfader** entra em *Manual override* (o módulo larga os controles).

### Em bloco: Rule for act

```text
Rule for act: [Act 4 ▾] [Cut on phrase ▾] [16 ▾] [Apply to act]
```

O §16 pensa em blocos (*"viradas longas no melódico, rápidas nos flashes"*). Em vez de 36 pares na
mão, aplique a regra ao ato inteiro. **Precedência: `par > ato > automático`** — um par só declara o
que **discorda** do ato (se o ato manda `Cut / 16` e o par diz só `Bass Swap`, fica *Bass Swap com 16
compassos*). Aplicar uma regra **não apaga** os ajustes de par.

### Zerar tudo: Reset all edits

Devolve **todas** as transições ao **Automatic** de uma vez — apaga overrides de par **e** regras de
ato (com confirmação).

> São **dois armazéns**: *Clear snapshots* limpa o **cache de análise**; *Reset all edits* limpa as
> **suas escolhas**. Regenerar ou reler a biblioteca **nunca** mexe nas suas escolhas — de propósito.

---

## 🔊 Tocar o set — Run set

O momento. O módulo entrega de uma faixa para a outra sozinho. O seletor ao lado escolhe **o quanto**
rodar:

| Escolha | Para quê |
|---|---|
| **Whole set** | o set inteiro, Ato 1 → 7 |
| **Act N** | ensaiar um ato isolado |
| **Act N → N+1** | ensaiar a **virada** entre atos (§19) |

Ensaiar a passagem do Ato 4 pro 5 sem isso exigiria rodar ~50 min até chegar lá. O trecho é compilado
**como um set próprio**: abre no deck 1 e não tenta entregar pra faixa que não está nele.

**Controles:** Pause / Resume / Skip / Stop set. **Mexer no crossfader** → *Manual override* (devolve
os decks). **Fechar a janela não para o set** — ele foi feito pra sobreviver à janela.

> 🎯 **A regra vale por faixa:** cada uma **começa no Enter @** dela e **entrega no Exit @** dela.
> Única exceção: a **primeira faixa do que você roda** não tem ninguém entregando pra ela, então
> começa na entrada automática (no set inteiro é a abertura; num ensaio de ato, a 1ª faixa do ato).
>
> ⏱️ **Preview × Run set — os pontos são os mesmos, muda o *quando*.** O Preview **salta** as decks
> pros pontos e toca só o trecho. O Run set toca a faixa **desde o começo** e só vira quando ela
> **chega** no Exit @. A faixa que espera fica **pré-posicionada no Enter @** assim que carrega, pra
> você ver onde ela entra. Se parecer que "não aplicou", confira no log: `Transition N -> M "<tipo>"`
> — o horário bate com o Exit @.

---

## 🪄 Receitas rápidas

O que você vai querer no calor do momento:

| Quero… | Faça |
|---|---|
| **Corte seco imediato** | Bars `8` + **Cut on phrase** |
| **Esperar o drop pra virar** | **Exit @ → drop** na faixa que sai |
| **Trazer a faixa já no drop** | **Enter @ → drop** na faixa que entra |
| **Faixa com intro longa** | **Enter @** na linha dela (ex.: `0:48`) |
| **Flash de 90 s** | **Exit @** `1:30` na faixa |
| **Mescla rápida** | Bars `8–16` + **EQ Blend** |
| **Mescla longa e suave** | Bars `64` + **EQ Blend / Breakdown swap** |
| **Um ato inteiro mais rápido** | **Rule for act** → Bars `16` → **Apply to act** |
| **Ensaiar a virada Ato 4→5** | **Run set** → `Act 4 → 5` |
| **Ouvir como a faixa entra** | selecione ela → **Preview on decks** |
| **Zerar todos os ajustes** | **Reset all edits** |

> **Sua escolha vence as heurísticas** — só a física discute: se a virada não cabe antes da faixa
> acabar, ela é aparada e o relatório avisa (`… trimmed to N`). Nada acontece calado.

---

## 🆘 Quando trava

**Energy / Sections / Exit @ vazios e tudo vira "Auto DJ fallback"**
Falta **waveform**. Analise a faixa pela biblioteca do Mixxx (clique-direito → *Analisar*) e clique
**Read native analysis**.

**Mudei o build e as transições não mudaram**
Reabrir só relê o gravado. Clique **Read native analysis** pra recalcular.

**Ajustei uma transição e ela voltou ao automático**
O ajuste é por **par de faixas**. Se você reordenou e aquelas duas não são mais vizinhas, ele fica
salvo mas inerte — volta a valer se o par voltar. Pra desfazer de vez, escolha **Automatic**.

**Forcei 64 compassos e saiu menos**
Não cabia antes da faixa acabar (`… trimmed to N`). Use um número menor ou uma faixa com mais cauda.

**Limpei a biblioteca, mas aparecem faixas antigas**
Snapshots vivem no **sidecar**. Clique **Read native analysis** (ele remove as sumidas) ou **Clear
snapshots** pra reset total.

**BPM absurdo (ex.: 187.5 numa faixa de 112)**
É o **beatgrid do Mixxx** errando. O motor reage certo (avisa e usa **Cut on phrase**), mas conserte
o beatgrid ou tire a faixa.

**Áudio picotando**
É a saída, não o módulo. Veja *setup*: placa física, WASAPI, sample rate casada — e, se só picota com
os dois decks, troque a *Trava de Tom* de **Rubberband** para **Soundtouch**.

---

## 🔧 Nos bastidores (pra quem gosta do porquê)

Nada disto é preciso pra tocar — mas explica por que o módulo age assim:

- **A ordem do plano lidera** porque comparar por score já foi tentado e saiu pior: o score mede
  justo o que o roteiro **não** otimiza (o Ato 1 real é ruim pela roda Camelot, mas foi curado por
  ouvido). O motor informa, o DJ decide.
- **Breakdown swap antes de Cut**: o §16 nunca pede corte pra tom incompatível — pede *"troca por
  breakdown"*, trazendo a faixa nova por um trecho de baixa energia onde quase não há tom pra brigar.
- **Beat sync pelo plano**: só o planejador sabe se os tempos casam; derivar do tipo dava batida
  torta.
- **Não há mais seletor de curva de energia**: com o roteiro fixando a forma (e a energia sendo um
  **platô** nas referências, não uma curva), ele só mexia no número de *energy fit*, nunca na ordem —
  então prometia o que não fazia.
- **Ainda não existe**: timeline arrastável, desenho de curva à mão, stems (§27, Fase 10 pós-MVP).

---

## 💾 Onde ficam os dados

- **Sidecar**: `%LOCALAPPDATA%\Mixxx\music-sync-dj.sqlite` (schema **v9**) — snapshots, suas escolhas
  de transição e settings do módulo. Separado da biblioteca; apagá-lo só perde os snapshots (é só
  re-ler).
- A biblioteca, os waveforms e a análise nativa são do **Mixxx** — o módulo lê, não reescreve.
