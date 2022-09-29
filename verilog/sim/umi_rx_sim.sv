module umi_rx_sim (
	input clk,
	output [255:0] packet,
	input ready,
	output valid
);

	sb_rx_sim rx_i (
		.clk(clk),
		.data(packet),
		.dest(),
		.last(),
		.ready(ready),
		.valid(valid)
	);

	// handle differences between simulators

	`ifdef __ICARUS__
		task init(input string uri);
			rx_i.init(uri);
		endtask
	`else
		function void init(input string uri);
			/* verilator lint_off IGNOREDRETURN */
			rx_i.init(uri);
			/* verilator lint_on IGNOREDRETURN */
		endfunction
	`endif

endmodule