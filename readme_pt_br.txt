<div class="moz-text-flowed" style="font-family: -moz-fixed">
      Servidor de banco de dados Leto � um servidor de banco de dados de v�rias
      plataformas ou um banco de dados com um sistema de gerenciamento.
      Sistema destinado a Programas do Cliente escritos em x[H]arbour,
      poder� trabalhar com arquivos DBF/CDX, localizados em um servidor remoto.

      1. Directory structure

      bin/          -    Arquivo execut�vel do servidor
      doc/          -    documenta��o
      include/      -    arquivos de cabe�alho de origem
      lib/          -    lib(Biblioteca) Rdd
      source/
          client/   -    fontes de acesso CLIENTE do rdd
          common/   -    Alguns arquivos de origem comuns
          client/   -    fontes de acesso SERVIDOR do Rdd
      tests/        -    Progamas de exemplos de uso do RDD
      utils/
          manage/   -    Utilitario de acesso ao servidor


      2. Criando bin�rios

      2.1 Compilador Borland Win32 C

      Uma vari�vel de ambiente, que define um caminho para o diret�rio x[H]arbour,
      HB_PATH deve ser definida antes de compilar a LIB.
      Isso pode ser feito, por exemplo, adicionando uma linha em um make_b32.bat ou no PATH:

      SET HB_PATH=c:\harbour

      Se voc� usar xHarbour, tire coment�rios de uma linha ' XHARBOUR = yes ' em Makefile.BC.
      Em seguida, executar o make_b32.bat e letodb.exe arquivo execut�vel do servidor
      ser� exibida em uma bin/diret�rio e rdd rddleto.lib biblioteca em um lib/diret�rio.

      2.2 Compilador Linux GNU C

      Uma vari�vel de ambiente, que define um caminho para o diret�rio Harbour,
      HB_ROOT deve ser definida antes compilando.
      Ou apenas altere o valor de HRB_DIR na Makefile.Linux.

      Em seguida, executar o make_linux.sh e letodb arquivo execut�vel do servidor
      ser� exibida em uma bin/diret�rio e rdd librddleto.a biblioteca em um lib/diret�rio.

      2.3 xHarbour Builder(Comercial)

      Execute o make_xhb.bat para construir bin�rios com este compilador.
      Provavelmente, voc� precisar� alterar o caminho para sua c�pia Construtor
      de express�es na make_xhb.bat xHarbour. O valor padr�o �:

      set XHB_PATH=c:\xhb

      3. Executando e parando servidor

      Apenas execut�-lo:

      letodb.exe                    ( under Windows )
      ./letodb                      ( under Linux )

      Para o desligamento do servidor, execute o arquivo execut�vel mesmo com um par�metro 'stop'

      letodb.exe stop               ( under Windows )
      ./letodb stop                 ( under Linux )

      4. Configura��o do servidor

      4.1 letodb.ini

      Voc� pode fornecer configura��o arquivo letodb.ini se voc� n�o estiver
      satisfeito com valores par�metros padr�o.
      No momento existem os seguintes par�metros (padr�o os valores s�o designados):

      Port = 2812              -    Numero da porta do servidor
      DataPath = \dados        -    Um caminho para um diret�rio de dados(DBF) em um servidor, esse caminho � valido apartir de onde o server do letoDB esta iniciando
      Logfile = "letodb.log"   -    caminho e o nome de um arquivo de log, nesse exemplo esta criando no mesmo diretorio onde o letodb.exe esta iniciando
      Default_Driver = CDX     -    Padr�o RDD para abrir arquivos no servidor ( CDX/NTX ) o Padr�o � CDX
      Lower_Path = 0           -    Caso seja definido como 1, vai converter todos os caminhos para min�sculas
      EnableFileFunc = 0            Caso seja definido como 1, vai Ativar o uso de fun��es como( leto_file(),leto_ferase(), leto_frename() )
      EnableAnyExt = 0         -    Caso seja definido como 1, � Ativado a cria��o de arquivos de dados(DBF) e �ndices com qualquer extens�o, com exce��o do padr�o (DBF, CDX, NTX)
      Pass_for_Login = 0       -    Caso seja definido como 1, a autentica��o do usu�rio � necess�ria para acessar o servidor
      Pass_for_Manage = 0      -    Caso seja definido como 1, a autentica��o do usu�rio � necess�ria para usar fun��es de gest�o (Leto_mggetinfo (), etc)
      Pass_for_Data = 0        -    Caso seja definido como 1, a autentica��o do usu�rio � necess�rio ter acesso de escrita aos dados
      Pass_File = "leto_users" -    o caminho e o nome do arquivo com as informa��es dos usu�rios
      Crypt_Traffic = 0        -    Caso seja definido como 1, passa os dados pela a rede cripitografado

      � poss�vel definir estrutura [DATABASE] (Banco de dados) se voc� precisa ter
      um diret�rio, onde arquivos s�o abertos por outro RDD:

      [DATABASE]
      DataPath =               -    (Op��o obrigat�ria)
      Driver = CDX             -    ( CDX/NTX )

      Voc� pode definir quantas se��es [DATABASE], conforme necess�rio.

      No Ambiente Windows o letodb.ini deve ser colocado em um diret�rio,
      de onde servidor � iniciado.

      No Linux o programa procura-lo no diret�rio de onde o servidor
      � iniciado e se n�o tiver sucesso, vai procurar no diret�rio /etc.


      4.2 Autentica��o

      Para ativar autentica��o do subsistema voc� precisa configurar um dos seguintes par�metros em
      letodb.ini a 1� �: Pass_for_Login, Pass_for_Manage, Pass_for_Data.
      Mas antes voc� precisa criar pelo menos um usu�rio com direitos administrador, porque quando autentica��o
      subsistema funcionar, apenas usu�rios autenticados com admin direitos s�o capazes de acrescentar / alterar os usu�rios
      e as senhas.
      Para adicionar um usu�rio, voc� precisar� incluir uma chamada de LETO_USERADD () no seu lado cliente
      programa, por exemplo:
      
      LETO_USERADD( "admin", "secret:)", "YYY" )

      Onde "YYY" � uma STRING, o que confere direitos de administrador, gerenciar e escrever acesso. Voc� pode
      tamb�m utilizar o utils/manager/console.prg programa para definir ou alterar dados de autentica��o.

      Para se conectar a um servidor com uma autentica��o de dados (nome de usu�rio e senha), voc� precisar�
      utiliza��o da Fun��o LETO_CONNECT()
      Abaixo um exemplo de Conex�o:

      FUNCTION MAIN()
      IF ( leto_Connect( "//192.168.243.16/","USUARIO","SENHA" ) ) == -1
         nRes := leto_Connect_Err()
         IF nRes == LETO_ERR_LOGIN
            ? "Falha ao Logar"
         ELSEIF nRes == LETO_ERR_RECV
            ? "Error ao conectar"
         ELSEIF nRes == LETO_ERR_SEND
            ? "Erro de envio"
         ELSE
            ? "N�o connectado"
         ENDIF

         Return Nil
      ENDIF
      ? "Conectado em " + leto_GetServerVersion()
      INKEY(0)
      leto_DisConnect()
      RETURN
  
      5. A conex�o com o servidor a partir de programas do cliente.

      Para poder se conectar ao servidor voc� precisar� vincular a rddleto.lib (Windows)
      ou librddleto.a (Linux) e no seu Aplicativo ten que adicionar no in�cio do seu .PRG
      principal duas linhas:

      REQUEST LETO
      RDDSETDEFAULT( "LETO" )

      Para abrir um arquivo DBF em um servidor, voc� precisar� escrever em uma
      instru��o SET PATH TO ou no comando use diretamente um caminho para
      o servidor em um formul�rio padr�o //ip_address:Port/data_path/Nome_Arquivo.

      Se um par�metro 'datapath' de um arquivo de configura��o do servidor � definido
      como um valor n�o vazio, voc� precisar� designar n�o o caminho completo
      para um arquivo no servidor, mas somente uma relativa (relativamente a ' Datapath').

      Por exemplo, se voc� precisar abrir um arquivo Test.dbf, que est� localizado
      no valor de par�metro (de um arquivo de configura��o do servidor letodb.ini)
      Servidor 192.168.5.22 em um diret�rio /dados Mydir e o 'datapath' � ' / Dados ',
      a sintaxe ser� o seguinte:

      USE "//192.168.5.22:2812/mydir/test"

      Se o servidor n�o executa ou voc� escrever um caminho errado, voc� ter� erros de execu��o.
      � poss�vel verificar acessibilidade de um servidor antes de abrir arquivos usando
      a fun��o leto_Connect (cAddress), que retorna "-1" no caso de tentativa sem �xito:

      Se o servidor n�o executar ou voc� escreve um caminho errado, voc� ter� erros de execu��o.
      � poss�vel verificar a acessibilidade de um servidor antes de abrir arquivos usando
      a fun��o leto_Connect (cAddress) , que retorna -1 no caso de tentativa sem �xito:

      Exemplo:
      IF leto_Connect( "//192.168.5.22:2812/mydir/" ) == -1
         Alert( "Can't connect to server ..." )
      ENDIF

      6. Utilit�rio de gerenciamento.

      H� dois utilit�rios de gerenciamento, interface gr�fica com o usu�rio
      e do console, as fontes est�o no diret�rio utils/manage.

      O utilit�rio GUI, Manage.prg, � feita com a biblioteca HwGUI.
      Se tiver HwGUI, basta escrever na linha ' set HWGUI_INSTALL= ""
      do arquivo utils/Manage/BLD.bat um caminho para o diret�rio HwGUI
      e executar o BLD.bat, ele criar� Manage.exe para voc�.

      Para aqueles, que n�o usam HwGUI, h� um utilit�rio Modo de console,
      console.prg. Para criar um console.exe, voc� pode usar para criar programas
      o hbmake do x[H]arbour e o hbmk2 no Harbour, voc� precisa apenas adicionar rddleto.lib � lista de bibliotecas externas.
      Execute o console.exe com um servidor Nome ou IP e porta como um par�metro:

      Exexmplos:
      console.exe server_name:nPort
      console.exe ip_address:nPort
      console.exe 192.168.254.17:2812

      O nome_do_servidor e endere�o_IP em uma linha de comando devem
      ser sem zeros � esquerda barras ('//'), pois interpreta Clipper/Harbour
      de maneira especial.

      7. Algumas observa��es extras:
      - O letodb.exe que vai rodar como um servi�o no servidor usando a porta 2812, ent�o tem
      que liberar essa porta no modem do servidor dando redirecionamento para o IP do servidor LETODB.
      Esse letodb.exe pode rodar de qualquer lugar ou seja qualquer pasta do servidor.
      - Ao finalizar o Aplicativo deve encerrar a conex�o com LETO_DISCONNECT()
      - Incluir em Pontos estrat�gico do sistema o uso de leto_BeginTransaction(), leto_Rollback() e
      leto_CommitTransaction() na pasta \letodb\tests\ tem exemplo de uso.


Nota: Texto originalmente escrito por Alexander Kresin e traduzido por Leonardo Machado </div>