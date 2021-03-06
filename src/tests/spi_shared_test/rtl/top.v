module top
(
  clk_in,
  reset_n_in,
  // Global data
  sck0_in,
  sdi0_in,
  cs0_n_in,
  // Daisy data
  sck1_in,
  sdi1_in,
  sdo1_out,
  cs1_n_in,
  // Success flags
  ready_n_ts_out,
  // Indicators
  status_led_n_out
);

    input wire clk_in;
    input wire reset_n_in;
    input wire sck0_in;
    input wire sdi0_in;
    input wire cs0_n_in;
    input wire sck1_in;
    input wire sdi1_in;
    output wire sdo1_out;
    input wire cs1_n_in;
    output wire ready_n_ts_out;
    output wire status_led_n_out;

    reg[2:0] sck0;
    reg[1:0] sdi0;

    reg[2:0] sck1;
    reg[1:0] sdi1;
    
    wire sck0_rising_edge;
    wire sck1_rising_edge;

    assign sck0_rising_edge = ~sck0[2] & sck0[1];
    assign sck1_rising_edge = ~sck1[2] & sck1[1];

    reg[31:0] shared_buffer;

    // Synchronzing data clocks
    always @(posedge clk_in)
      begin
        sck0 <= {sck0[1], sck0[0], sck0_in};
        sck1 <= {sck1[1], sck1[0], sck1_in};

        sdi0 <= {sdi0[0], sdi0_in};
        sdi1 <= {sdi1[0], sdi1_in};
      end

    // SPI logic -- SPI0 has precedence over SPI1
    always @(posedge clk_in)
      begin
          if (!reset_n_in)
            shared_buffer <= 0;
          else if (!cs0_n_in && sck0_rising_edge)
            shared_buffer <= { shared_buffer[30:0], sdi0[1] };
          else if (!cs1_n_in && sck1_rising_edge)
            shared_buffer <= { shared_buffer[30:0], sdi1[1] };
      end

    // Heartbeat

    reg[22:0] heartbeat_count = 0;

    always @(posedge clk_in, negedge reset_n_in)
      begin
        if (!reset_n_in)
          heartbeat_count = 0;
        else
          heartbeat_count = heartbeat_count + 1;
      end

    assign sdo1_out = shared_buffer[31];

    assign status_led_n_out = heartbeat_count[22] ^ ~((sck0_in & ~cs0_n) | (sck1_in & ~cs1_n));


endmodule